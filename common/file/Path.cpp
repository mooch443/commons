#include "Path.h"
#include <cstdlib>
#if WIN32 && !defined(__EMSCRIPTEN__)
#include <misc/dirent.h>
#define OS_SEP '\\'
#define NOT_OS_SEP '/'
#elif !defined(__EMSCRIPTEN__)
#include <dirent.h>
#define OS_SEP '/'
#define NOT_OS_SEP '\\'
#endif

#if defined(__EMSCRIPTEN__)
#define OS_SEP '/'
#define NOT_OS_SEP '\\'
#endif

#include <errno.h>

#ifdef __APPLE__
#include <Availability.h>

#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101500
#undef __cpp_lib_filesystem
#endif
#endif
#endif

#ifdef __cpp_lib_filesystem
#include <filesystem>
#endif

#include <misc/checked_casts.h>
#include <misc/metastring.h>
#include <sys/stat.h>
#include <misc/detail.h>

namespace file {
    char Path::os_sep() { return OS_SEP; }
    
    Path::Path(const std::string& s)
        : _str(s)
    {
        // remove trailing slashes
        while(utils::endsWith(_str, OS_SEP))
            _str = _str.substr(0, _str.length()-1);
		for (size_t i = 0; i < _str.length(); ++i)
			if (_str[i] == NOT_OS_SEP) _str[i] = OS_SEP;
    }

    Path::Path(const char* c)
        : Path(std::string(c))
    {}

    Path::Path(std::string_view sv)
        : Path(std::string(sv))
    {}

    Path& Path::operator/=(const Path &other) {
        _str += other.str();
        return *this;
    }

    Path Path::fromStr(const std::string& str) {
        return Path(Meta::fromStr<std::string>(str));
    }
    
    bool Path::is_absolute() const {
#if WIN32
        return (_str.length() >= 3 && _str[1] == ':');
#else
        return !_str.empty() && _str[0] == OS_SEP;
#endif
    }

std::string_view Path::filename() const {
        if(empty())
            return _str;
        
        const char *ptr = _str.data() + _str.length() - 1;
        for(; ptr >= _str.data(); ptr--) {
            if(*ptr == OS_SEP)
                return std::string_view(ptr+1u, size_t(_str.data() + _str.length() - (ptr+1u)));
        }
        
        return _str;
    }
    
    std::pair<Path, Path> Path::split(size_t position) const
    {
        size_t before_len = 0, count = 0;
        
        for (size_t i=0; i<=_str.length(); i++) {
            if(i == _str.length() || _str[i] == OS_SEP) {
                if(++count > position) {
                    before_len = i;
                    break;
                }
            }
        }
        
        if(count != position+1)
            throw U_EXCEPTION("Path only contains ",count," segments (requested split at ",position,").");
        
        return {Path(std::string_view(_str.data(), before_len)), Path(std::string_view(_str.data() + before_len, _str.length() - before_len))};
    }

    Path Path::remove_filename() const {
        return Path(std::string_view(_str.data(), _str.length() - filename().length()));
    }

    Path operator/(const Path& lhs, const Path& rhs) {
        return Path(lhs.str() + OS_SEP + rhs.str());
    }

    Path operator+(const Path& lhs, const Path& rhs) {
        return Path(lhs.str() + rhs.str());
    }

    bool Path::exists() const {
        return file_exists(_str);
    }

    uint64_t Path::file_size() const {
#if defined(WIN32) && !defined(__EMSCRIPTEN__)
        WIN32_FILE_ATTRIBUTE_DATA fInfo;

        DWORD ftyp = GetFileAttributesEx(c_str(), GetFileExInfoStandard, &fInfo);
        if (INVALID_FILE_ATTRIBUTES == ftyp || fInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            throw U_EXCEPTION("Cannot stat file ",str(),".");
        }
        return (static_cast<ULONGLONG>(fInfo.nFileSizeHigh) << sizeof(fInfo.nFileSizeLow) * 8) | fInfo.nFileSizeLow;
#elif defined(__EMSCRIPTEN__)
        //!TODO: [EMSCRIPTEN] can we do this?
        throw U_EXCEPTION("Cannot stat file in Emscripten.");
#else
        struct stat sbuf;
        if (stat(c_str(), &sbuf) == -1)
            throw U_EXCEPTION("Cannot stat file ",str(),".");
        return narrow_cast<uint64_t>(sbuf.st_size);
#endif
    }

    std::string_view Path::extension() const {
        if(empty())
            return std::string_view(_str);
        
        const char *ptr = &_str.back();
        for(; ptr != _str.data(); --ptr) {
            if (*ptr == '.') {
                return std::string_view(ptr+1, size_t(&_str.back() - ptr));
            }
        }
        
        return std::string_view(_str.data() + _str.length() - 1, 0);
    }
    
    bool Path::has_extension() const {
        return !extension().empty();
    }
    
    bool Path::create_folder() const {
        if(exists())
            return true;
        
        std::vector<std::string> folders;
        Path tmp = *this;
        while (!tmp.empty()) {
            if (tmp.exists()) {
                break;
            }
            
            folders.insert(folders.begin(), tmp.str());
            tmp = tmp.remove_filename();
        }
        
        for(auto &folder : folders) {
            int err = -1;
#if WIN32 && !defined(__EMSCRIPTEN__)
			if (!folder.empty() && folder.back() == ':')
				continue;
			if(!CreateDirectory(folder.c_str(), NULL)) {
#elif defined(__EMSCRIPTEN__)
            { //!TODO: [EMSCRIPTEN]
#else
            err = mkdir(folder.c_str(), ACCESSPERMS);
            if(err != 0) {
#endif
                FormatExcept("Cannot create folder ",folder," (error: ",errno,").");
                return false;
            }
        }
        
        return true;
    }
    
    bool Path::is_folder() const {
#if WIN32 && !defined(__EMSCRIPTEN__)
        DWORD ftyp = GetFileAttributesA(str().c_str());
        if (ftyp == INVALID_FILE_ATTRIBUTES)
            return false;  //something is wrong with your path!

        if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
            return true;   // this is a directory!

        return false;
#elif defined(__EMSCRIPTEN__)
        //!TODO: [EMSCRIPTEN]
        return false;
#else
        struct stat path_stat;
        if(stat(empty() ? "/" : str().c_str(), &path_stat) != 0)
            return false;
        return S_ISDIR(path_stat.st_mode);
#endif
    }
    
    bool Path::is_regular() const {
        struct stat path_stat;
        if(stat(str().c_str(), &path_stat) != 0)
            return false;
        return S_ISREG(path_stat.st_mode);
    }
    
    bool Path::delete_file() const {
        if(!exists())
            return false;
        
        if(!is_regular())
            throw U_EXCEPTION("Cannot delete non-regular file ",str(),".");
        
        return std::remove(str().c_str()) == 0;
    }
    
    bool Path::delete_folder(bool recursively) const {
        if(!exists())
            return false;
        
        if(!is_folder())
            throw U_EXCEPTION("Cannot folder-delete a non-folder ",str(),".");
        
        if(recursively) {
            auto files = find_files();
            for(auto &file : files) {
                if(file.is_folder()) {
                    if(!file.delete_folder(true))
                        throw U_EXCEPTION("Cannot delete folder ",file.str(),".");
                    
                } else if(file.is_regular()) {
                    if(!file.delete_file())
                        throw U_EXCEPTION("Cannot delete file ",file.str(),".");
                    
                } else
                    throw U_EXCEPTION("Unknown file type ",file.str(),".");
            }
        }
#if defined(WIN32) && !defined(__EMSCRIPTEN__)
        return RemoveDirectory(str().c_str());
#elif defined(__EMSCRIPTEN__)
        throw U_EXCEPTION("Emscripten cannot delete folders.");
#else
        return rmdir(str().c_str()) == 0;
#endif
    }
        
        bool valid_extension(const file::Path& path, const std::string& filter_extension) {
            if(filter_extension.empty())
                return true;
            
            auto extensions = utils::split(utils::lowercase(filter_extension), ';');
            if(path.has_extension()) {
                return contains(extensions, utils::lowercase((std::string)path.extension()));
            }
            
            return false;
        }
    
    std::set<Path> Path::find_files(const std::string& filter_extension) const {
#if defined(__EMSCRIPTEN__)
        //!TODO: [EMSCRIPTEN]
        throw U_EXCEPTION("Cannot iterate folders in Emscripten.");
#else
        if(!is_folder())
            throw U_EXCEPTION("The path ",str()," is not a folder and can not be iterated on.");
        if(!empty() && !exists())
            throw U_EXCEPTION("The path ",str()," does not exist.");
        
        std::set<Path> result;
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir (empty() ? "/" : str().c_str())) != NULL) {
            while ((ent = readdir (dir)) != NULL) {
                std::string file(ent->d_name);
                if(file == "." || file == "..")
                    continue;
                
                if(ent->d_type & DT_DIR || valid_extension(file::Path(ent->d_name), filter_extension))
                    result.insert(*this / ent->d_name);
            }
            closedir (dir);
            
        } else
            throw U_EXCEPTION("Folder ",str()," exists but cannot be read.");
        
        return result;
#endif
    }

    Path Path::replace_extension(std::string_view ext) const {
        auto e = extension();
        return std::string_view(_str.data(), size_t(max(0, e.data() - _str.data() - 1))) + "." + ext;
    }

    Path Path::add_extension(std::string_view ext) const {
        auto current = extension();
        if(ext != current)
            return Path(_str + "." + ext);
        return *this;
    }
    
    Path Path::remove_extension() const {
        if(has_extension())
            return Path(_str.substr(0, _str.length() - extension().length() - 1));
        return *this;
    }
    
    int copy_file( const char* srce_file, const char* dest_file )
    {
#ifndef __cpp_lib_filesystem
        try {
            std::ifstream srce( srce_file, std::ios::binary ) ;
            std::ofstream dest( dest_file, std::ios::binary ) ;
            dest << srce.rdbuf() ;
        } catch (std::exception& e) {
            FormatExcept("Caught an exception copying '", srce_file,"' to '", dest_file,"': ",e.what(),"");
            return 1;
        }
        
        return 0;
#else
        try {
            std::filesystem::copy(srce_file, dest_file);
            
        } catch(const std::filesystem::filesystem_error& e) {
            FormatExcept("Caught an exception copying '",srce_file,"' to '",dest_file,"': ", e.what());
            return 1;
        }
        return 0;
#endif
    }
    
    bool Path::move_to(const file::Path &to) {
#ifndef __cpp_lib_filesystem
        if(std::rename(c_str(), to.c_str()) == 0) {
#else
        try {
            std::filesystem::rename(c_str(), to.c_str());
#endif
            assert(!exists() && to.exists());
            return true;
#ifdef __cpp_lib_filesystem
        } catch (std::filesystem::filesystem_error& e) {
            // do nothing
    #ifndef NDEBUG
            print("Filesystem error: '",e.what(),"'");
    #endif
#endif
        }
        
        if(copy_file(c_str(), to.c_str()) != 0) {
            FormatExcept("Failed renaming file ",str()," to ",to.str(),", so I tried copying. That also failed. Make sure the second location is writable (this means that the file is still available in the first location, it just failed to be moved).");
            
        } else {
            if(!delete_file())
                FormatWarning("Cannot remove file ",str()," from its original location after moving.");
            return true;
        }
        
        return false;
    }
    
    bool Path::operator<(const Path& other) const {
        return str() < other.str();
    }
    
    bool Path::operator<=(const Path& other) const {
        return str() <= other.str();
    }
    
    bool Path::operator>(const Path& other) const {
        return str() > other.str();
    }
    
    bool Path::operator>=(const Path& other) const {
        return str() >= other.str();
    }
    
    bool operator==(const Path& lhs, const Path& rhs) {
        return lhs.str() == rhs.str();
    }
    
    bool operator!=(const Path& lhs, const Path& rhs) {
        return lhs.str() != rhs.str();
    }
    
    FILE* Path::fopen(const std::string &access_rights) const {
#if WIN32 && !defined(__EMSCRIPTEN__)
        FILE* f = nullptr;
        ::fopen_s(&f, c_str(), access_rights.c_str());
#elif defined(__EMSCRIPTEN__)
        //!TODO: [EMSCRIPTEN] can it?
        FILE* f = nullptr;
        throw U_EXCEPTION("Emscripten cannot open files.");
#else
        auto f = ::fopen(c_str(), access_rights.c_str());
#endif
        if(!f)
            FormatError("fopen failed, errno = ", errno);
        return f;
    }
        
#if defined(WIN32) && !defined(__EMSCRIPTEN__)
#define pclose _pclose
#define popen _popen
#endif
        
    std::string exec(const char* cmd) {
        std::array<char, 256> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        if(!result.empty() && result[result.length()-1] == '\n')
            result = result.substr(0,result.length()-1);
        if(!result.empty() && result[0] == '\"')
            result = result.substr(1);
        
        return result;
    }
}

std::ostream& operator<<(std::ostream& os, const file::Path& p) {
    return os << p.str();
}

