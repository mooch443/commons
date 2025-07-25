#include "Path.h"
#include <cstdlib>
#if WIN32 && !defined(__EMSCRIPTEN__)
#include <direct.h>
#define PATH_BUF_SIZE MAX_PATH
#define OS_SEP '\\'
#define NOT_OS_SEP '/'
#else//if !defined(__EMSCRIPTEN__)
#include <dirent.h>
#define PATH_BUF_SIZE PATH_MAX
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

#if __cplusplus >= 201703L && defined(__has_include) && __has_include(<filesystem>)
#include <filesystem>
#define USE_STD_FILESYSTEM
#elif defined(__cpp_lib_filesystem)
#undef __cpp_lib_filesystem
#endif

#include <sys/stat.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#endif

namespace cmn::file {
#ifdef USE_STD_FILESYSTEM
Path Path::absolute() const {
    if(_stat_cache.absolute.has_value() && not _stat_cache.too_old()) {
        return _stat_cache.absolute.value();
    }
    
    std::filesystem::path path(_str);
    path = std::filesystem::absolute(path).string();
    _stat_cache.absolute = path.string();
    _stat_cache.update();
    return _stat_cache.absolute.value();
}
std::optional<Path> Path::canonical() const {
    try {
        std::filesystem::path path(_str);
        return Path(std::filesystem::weakly_canonical(path).string());
    } catch(const std::exception& ex) {
#ifndef NDEBUG
        FormatExcept("Cannot turn ", *this, " into canonical form: ", ex.what());
#endif
        return std::nullopt;
    }
}
#else
    std::string getCanonicalPath(const std::string& filePath) {
        std::vector<std::string> pathComponents;
        std::string normalizedFilePath = filePath;
        std::istringstream iss(normalizedFilePath);
        std::string component;
        std::string driveLetter;

#if defined(_WIN32) || defined(_WIN64)
        std::replace(normalizedFilePath.begin(), normalizedFilePath.end(), '/', '\\');
        if (normalizedFilePath.length() >= 2 and normalizedFilePath[1] == ':') {
            driveLetter = normalizedFilePath.substr(0, 2);
            normalizedFilePath = normalizedFilePath.substr(2);
            iss.str(normalizedFilePath);
        }
#endif

        while (std::getline(iss, component, OS_SEP)) {
            if (component == "..") {
                if (not pathComponents.empty()) {
                    pathComponents.pop_back();
                }
            }
            else if (not component.empty() and component != ".") {
                pathComponents.push_back(component);
            }
        }

        std::ostringstream oss;
        if (not driveLetter.empty()) {
            oss << driveLetter;
        }

        if (filePath[0] == OS_SEP or not driveLetter.empty()) {
            oss << OS_SEP;
        }

        for (size_t i = 0; i < pathComponents.size(); ++i) {
            oss << pathComponents[i];
            if (i < pathComponents.size() - 1) {
                oss << OS_SEP;
            }
        }

        return oss.str();
    }

    std::string joinPaths(const std::string& basePath, const std::string& relPath) {
        std::string driveLetter;

#if defined(_WIN32) || defined(_WIN64)
        if (relPath.length() >= 2 and relPath[1] == ':') {
            return getCanonicalPath(relPath);
        }
#endif

        return getCanonicalPath(basePath + std::string(1, OS_SEP) + relPath);
    }

    Path Path::absolute() const {
#if defined(_WIN32) || defined(_WIN64)
        if ((_str.length() >= 2 and _str[1] == ':') or (_str.length() >= 1 and _str[0] == OS_SEP)) {
            return Path(getCanonicalPath(_str));
        }
#else
        if (_str.length() >= 1 and _str[0] == OS_SEP) {
            return Path(getCanonicalPath(_str));
        }
#endif

        std::string currentWorkingDirectory = cwd().str();
        std::string fullPath = joinPaths(currentWorkingDirectory, _str);
        return Path(getCanonicalPath(fullPath));
    }

#endif
    
    void Path::Stat::update() {
        if(assigned_at == 0u)
            assigned_at = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    bool Path::Stat::too_old() {
        /*if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - assigned_at.get() > std::chrono::seconds(60).count()) {
            assigned_at = 0u;
            exists.reset();
            is_folder.reset();
            return true;
        }*/
        return false;
    }

    Path::Path(const std::string& s)
        : Path(std::string_view(s))
    {
    }

    Path::Path(const char* c)
        : Path(std::string_view(c))
    {}

    Path::Path(std::string_view sv)
        : _str(std::string(sv))
    {
        for (size_t i = 0, N = _str.length(); i < N; ++i) {
            if(i + 1 == N) {
                // remove trailing slashes
                while(N > 1 && (_str.back() == OS_SEP || _str.back() == NOT_OS_SEP))
                {
                    _str.pop_back();
                    --N;
                }
                
                break;
            }
            
            if (_str[i] == NOT_OS_SEP) {
                _str[i] = OS_SEP;
                _stat_cache._lastSepPos = i;
            } else if(_str[i] == OS_SEP) {
                _stat_cache._lastSepPos = i;
            }
        }
    }

    Path& Path::operator/=(const Path &other) {
        //_str += other.str();
        *this = *this / other.str();
        return *this;
    }

    bool Path::is_absolute() const noexcept {
#if WIN32
        return _str[0] == OS_SEP || (_str.length() >= 3 && _str[1] == ':');
#else
        return !_str.empty() && _str[0] == OS_SEP;
#endif
    }

    std::string Path::filename() const {
        if(empty())
            return _str;
        
        const char *ptr = _str.data() + _str.length() - 1;
        for(; ptr >= _str.data(); ptr--) {
            if(*ptr == OS_SEP)
                return (std::string)std::string_view(ptr+1u, size_t(_str.data() + _str.length() - (ptr+1u)));
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
        // Handle the case where the right-hand side is an absolute path
        std::string_view rv(rhs.str());
        // Check if rhs is an absolute path
        if (not rv.empty() && rv.front() == OS_SEP) {
            throw std::invalid_argument("Cannot concatenate an absolute path to the right of any path.");
        }

        // Remove trailing separator from lhs if present
        std::string_view lv(lhs.str());
        if (not lv.empty() && lv.back() == OS_SEP) {
            lv = lv.substr(0, lv.length() - 1);
        }

        // Handle the case where lhs or rhs is empty
        if (lv.empty()) {
#if !defined(WIN32)
            return Path(std::string(1, OS_SEP) + std::string(rv));
#else
            return Path(std::string(1, OS_SEP) + std::string(rv));
#endif
        }
        if (rv.empty()) {
            return Path(std::string(lv));
        }

        // Concatenate and return the new path
        return Path(std::string(lv) + OS_SEP + std::string(rv));
    }

    /*Path operator+(const Path& lhs, const Path& rhs) {
        return Path(lhs.str() + rhs.str());
    }*/

    bool Path::exists() const {
        if (_stat_cache.exists.has_value())
            return _stat_cache.exists.value();

        //std::this_thread::sleep_for(std::chrono::milliseconds(size_t(float(rand()) / float(RAND_MAX) * 50 + 100)));
        _stat_cache.exists = file_exists(_str);
        _stat_cache.assigned_at = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return _stat_cache.exists.value();
    }

    std::vector<char> Path::retrieve_data() const {
#if defined(__EMSCRIPTEN__)
        if (exists()) {
            std::ifstream input(str(), std::ios::binary);
            if (!input.is_open())
                throw cmn::U_EXCEPTION("Cannot read file ", str());

            return std::vector<char>(std::istreambuf_iterator<char>(input), {});
        }
        else {
            struct Data {
                std::mutex _download_mutex;
                std::condition_variable _download_variable;
                std::atomic_bool _download_finished{ false };
                std::vector<char> data;
                file::Path _path;
            };

            static Data _data;

            std::unique_lock guard(_data._download_mutex);
            _data.data.clear();
            _data._path = *this;
            _data._download_finished = false;

            emscripten_async_wget_data(c_str(), (void*)&_data, [](void* arg, void* buffer, int size) {
                printf("Downloaded data (%d)", size);
                auto self = (Data*)arg;
                self->data.resize(size);
                std::copy((char*)buffer, (char*)buffer + size, self->data.data());
                self->_download_finished = true;
                self->_download_variable.notify_all();
                }, [](void* arg) {
                    auto self = (Data*)arg;
                    printf("Failed to download data for '%s'.\n", self->_path.c_str());
                });

            while (!_data._download_finished) {
                _data._download_variable.wait_for(guard, std::chrono::milliseconds(10));
                //emscripten_sleep(10);
            }

            return _data.data;
        }
#else
        std::ifstream input(str(), std::ios::binary);
        if (!input.is_open()) {
            throw cmn::U_EXCEPTION("Cannot read file ", str(), " cwd:", cwd(), " exists:", exists());
        }

        return std::vector<char>(std::istreambuf_iterator<char>(input), {});
#endif
    }

    std::string Path::read_file() const {
#if defined(__EMSCRIPTEN__)
        auto data = retrieve_data();
        return std::string(data.begin(), data.end());
#else
        std::ifstream input(str(), std::ios::binary);
        if(!input.is_open())
            throw cmn::U_EXCEPTION("Cannot read file ", str(), " cwd:", cwd(), " exists:", exists());
        
        std::stringstream ss;
        ss << input.rdbuf();
        
        return ss.str();
#endif
    }

    file::Path cwd() {
        char buffer[PATH_BUF_SIZE];
        std::string cwd;
#ifdef WIN32
        if(_getcwd(buffer, PATH_BUF_SIZE) != 0) {
#else
        if(getcwd(buffer, PATH_BUF_SIZE) != 0) {
#endif
            cwd = buffer;
        }
        return file::Path(cwd);
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
        
    uint64_t Path::last_modified() const {
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
            throw U_EXCEPTION("Cannot get file attributes for ", str());
            
        #ifdef __APPLE__ // macOS uses st_mtimespec instead of st_mtim
            return narrow_cast<uint64_t>(sbuf.st_mtimespec.tv_sec);
        #else
            #if defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500 || _POSIX_C_SOURCE >= 200809L
                // High resolution timestamp
                return narrow_cast<uint64_t>(sbuf.st_mtim.tv_sec);
            #else
                // Traditional timestamp
                rn narrow_cast<uint64_t>(sbuf.st_mtime);
            #endif
        #endif
    #endif
        }

    std::string_view Path::extension() const {
        if(empty())
            return std::string_view();
        
        const char *ptr = &_str.back();
        for(; ptr != _str.data(); --ptr) {
            if (*ptr == '.') {
                return std::string_view(ptr+1, size_t(&_str.back() - ptr));
            } else if(*ptr == OS_SEP) {
                return std::string_view();
            }
        }
        
        return std::string_view();
    }
    
    bool Path::has_extension(std::string_view ext) const {
        if(ext.empty())
            return not extension().empty();
        assert(utils::lowercase(ext) == ext);
        return utils::lowercase(extension()) == ext;
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
        if (_stat_cache.is_folder.has_value() && not _stat_cache.too_old()) {
            return _stat_cache.is_folder.value();
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(size_t(float(rand()) / float(RAND_MAX) * 500 + 100)));

        _stat_cache.is_folder = false;

#if WIN32 && !defined(__EMSCRIPTEN__)
        DWORD ftyp = GetFileAttributesA(str().c_str());
        if (ftyp == INVALID_FILE_ATTRIBUTES) {
            return false;  //something is wrong with your path!
        }
        if (ftyp & FILE_ATTRIBUTE_DIRECTORY) {
            _stat_cache.is_folder = true;
        }
//#elif defined(__EMSCRIPTEN__)
    //       //!TODO: [EMSCRIPTEN]
    //       return false;
#else
        struct stat path_stat;
        if(empty()
           || stat(str().c_str(), &path_stat) != 0)
        {
            // does not exist
        } else
            _stat_cache.is_folder = S_ISDIR(path_stat.st_mode);
#endif
        _stat_cache.update();
        return _stat_cache.is_folder.value();

    }
    
    bool Path::is_regular() const {
        if(empty())
            return false;
        
        if (_stat_cache.is_regular.has_value() && not _stat_cache.too_old()) {
            return _stat_cache.is_regular.value();
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(size_t(float(rand()) / float(RAND_MAX) * 500 + 100))); 

        _stat_cache.is_regular = false;
#if defined(_WIN32)
        DWORD dwAttrib = GetFileAttributes(str().c_str());
        if (dwAttrib != INVALID_FILE_ATTRIBUTES)
            _stat_cache.is_regular = (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        struct stat path_stat;
        if (empty()
            || stat(str().c_str(), &path_stat) != 0)
        {
            // does not exist
        }
        else _stat_cache.is_regular = S_ISREG(path_stat.st_mode);
#endif
        _stat_cache.update();
        return _stat_cache.is_regular.value();
    }
    
    bool Path::delete_file() const {
        if(!exists())
            return false;
        
        if(!is_regular())
            throw U_EXCEPTION("Cannot delete non-regular file ",str(),".");
        
        _stat_cache.exists.reset();
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
        //std::this_thread::sleep_for(std::chrono::milliseconds(size_t(float(rand()) / float(RAND_MAX) * 500 + 500)));

        if (not is_folder())
            throw U_EXCEPTION("The path " + str() + " is not a folder and cannot be iterated on.");
        if (not empty() && not exists())
            throw U_EXCEPTION("The path " + str() + " does not exist.");

        std::set<Path> result;

    #if defined(_WIN32)
        WIN32_FIND_DATA findFileData;
        auto str = this->str() + "\\*";
        HANDLE hFind = FindFirstFile(str.c_str(), &findFileData);

        if (hFind == INVALID_HANDLE_VALUE) {
            throw U_EXCEPTION("Folder " + str + " exists but cannot be read.");
        }
        else {
            do {
                const std::string file = findFileData.cFileName;
                if (file != "." && file != "..") {
                    // Assume valid_extension is some function to validate file extensions
                    if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || valid_extension(file, filter_extension)) {
                        result.insert(*this / file);
                    }
                }
            } while (FindNextFile(hFind, &findFileData) != 0);
            FindClose(hFind);
        }
    #else
        DIR* dir;
        struct dirent* ent;
        if ((dir = opendir(empty() ? "/" : str().c_str())) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                const std::string file = ent->d_name;
                if (file != "." && file != "..") {
                    if (ent->d_type & DT_DIR || valid_extension(file, filter_extension)) {
                        result.insert(*this / file);
                    }
                }
            }
            closedir(dir);
        }
        else {
            throw U_EXCEPTION("Folder " + str() + " exists but cannot be read.");
        }
    #endif
        return result;
    }

    Path Path::replace_extension(std::string_view ext) const {
        auto e = extension();
        return (std::string)std::string_view(_str.data(), size_t(max(0, e.data() - _str.data() - 1))) + "." + (std::string)ext;
    }

    Path Path::add_extension(std::string_view ext) const {
        auto current = extension();
        if(ext != current)
            return Path(_str + "." + (std::string)ext);
        return *this;
    }
    
    Path Path::remove_extension(const std::string& only) const {
        if(has_extension() && (only.empty() || extension() == only)) {
            return Path(_str.substr(0, _str.length() - extension().length() - 1));
        }
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
            std::filesystem::copy(srce_file, dest_file, std::filesystem::copy_options::overwrite_existing);
            
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
            _stat_cache.exists.reset();
#ifndef NDEBUG
            assert(!exists() && file::Path(to.str()).exists());
#endif
            return true;
#ifdef __cpp_lib_filesystem
        } catch (std::filesystem::filesystem_error& e) {
            // do nothing
    #ifndef NDEBUG
            Print("Filesystem error: '",e.what(),"'");
    #endif
#endif
        }
        
        if(copy_file(c_str(), to.c_str()) != 0) {
            FormatExcept("Failed renaming file ",str()," to ",to.str(),", so I tried copying. That also failed. Make sure the second location is writable (this means that the file is still available in the first location, it just failed to be moved).");
            
        } else {
            if(!delete_file())
                FormatWarning("Cannot remove file ",str()," from its original location after moving. This might mean that it has been left there and you will have to empty your temp folder manually.");
            return true;
        }
        
        return false;
    }
        
    FilePtr Path::fopen(const std::string& access_rights) const {
#if WIN32 && !defined(__EMSCRIPTEN__)
        FILE* fptr = nullptr;
        ::fopen_s(&fptr, c_str(), access_rights.c_str());
        FilePtr f{fptr};
#elif defined(__EMSCRIPTEN__)
        //!TODO: [EMSCRIPTEN] can it?
        FilePtr f;
        throw U_EXCEPTION("Emscripten cannot open files.");
#else
        FilePtr f{std::fopen(c_str(), access_rights.c_str())};
#endif
        if(!f)
            FormatError("fopen failed (", str(), "), errno = ", errno);
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
        
    void cd(const file::Path& path) {
#if defined(WIN32)
        if (SetCurrentDirectoryA(path.c_str()))
#else
        if (!chdir(path.c_str()))
#endif
            Print(" * cwd -> ", path.absolute());
        else {
            FormatError("Cannot change directory to ",path,". ");
        }
    }
        
        glz::json_t Path::to_json() const {
            return glz::json_t(c_str());
        }
}

std::ostream& operator<<(std::ostream& os, const cmn::file::Path& p) {
    return os << p.str();
}

