#pragma once
#include <commons.pc.h>

namespace file {
    using namespace cmn;

template<typename T, typename TIter = decltype(std::begin(std::declval<T>())),
         typename V = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T& iterable) {
    struct iterator {
        size_t i;
        TIter iter;
        bool operator != (const iterator& other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };
    struct iterable_wrapper {
        T& iterable;
        auto begin() { return iterator{0, std::begin(iterable)}; }
        auto end() { return iterator{0, std::end(iterable)}; }
    };
    return iterable_wrapper{iterable};
}

    class FilePtr {
    public:
        FilePtr() : file_(nullptr) {}
        
        explicit FilePtr(FILE* file)
            : file_(file)
        {}
        
        ~FilePtr() {
            if (file_) {
                std::fclose(file_);
            }
        }

        FilePtr(const FilePtr&) = delete;
        FilePtr& operator=(const FilePtr&) = delete;

        FilePtr(FilePtr&& other) noexcept : file_(other.file_) {
            other.file_ = nullptr;
        }

        FilePtr& operator=(FilePtr&& other) noexcept {
            if (this != &other) {
                if (file_) {
                    std::fclose(file_);
                }
                file_ = other.file_;
                other.file_ = nullptr;
            }
            return *this;
        }
        
        explicit operator bool() const {
            return file_ != nullptr;
        }
        
        bool operator==(const FilePtr& other) const {
            return file_ == other.file_;
        }

        bool operator!=(const FilePtr& other) const {
            return !(*this == other);
        }

        bool operator==(std::nullptr_t) const {
            return file_ == nullptr;
        }

        bool operator!=(std::nullptr_t) const {
            return file_ != nullptr;
        }
        
        // Reset the FilePtr to a new value
        void reset(FILE* newFile = nullptr) {
            if (file_) {
                std::fclose(file_);
            }
            file_ = newFile;
        }

        // Assignment operator for nullptr
        FilePtr& operator=(std::nullptr_t) {
            reset();
            return *this;
        }
        
        FILE* get() const {
            return file_;
        }

    private:
        FILE* file_;
    };
    
    class Path {
    public:
        //using ManagedFile = std::unique_ptr<FILE, decltype(&fclose)>;

    protected:
        //! The full path without any trailing slashes,
        //  but with file extension and filename
        GETTER(std::string, str);

        mutable struct Stat {
            std::optional<bool> exists, is_folder, is_regular;
            std::optional<std::string> absolute;
            timestamp_t assigned_at{ 0u };
            size_t _lastSepPos{std::string::npos};
            bool too_old();
            void update();
        } _stat_cache;
        
    public:
        Path(const std::string& s = "");
        Path(const char* c);
        Path(std::string_view sv);
        
        // Accessors
        const char* c_str() const noexcept { return _str.c_str(); }
        
        //! Concatenation
        //  Path p = "Users" / username / "Desktop"
        Path& operator/=(const Path&);
        Path& append(const Path&);
        
        //! Comparison operators:
        auto operator<=>(const Path& other) const noexcept {
            return _str <=> other._str;
        }
        bool operator==(const Path& other) const noexcept {
            return _str == other._str;
        }
        bool operator!=(const Path& other) const noexcept {
            return _str != other._str;
        }
        
        constexpr static char os_sep() noexcept {
#if WIN32 && !defined(__EMSCRIPTEN__)
            return '\\';
#else
            return '/';
#endif
        }
        constexpr static char not_os_sep() noexcept {
#if WIN32 && !defined(__EMSCRIPTEN__)
            return '/';
#else
            return '\\';
#endif
        }
        [[nodiscard]] bool is_absolute() const noexcept;
        [[nodiscard]] bool empty() const noexcept { return _str.empty(); }
        
        //! Returns the last filename, removes the path
        std::string filename() const;
        
        //! Returns path, removes last filename
        [[nodiscard]] Path remove_filename() const;
        
        std::pair<Path, Path> split(size_t position) const;
        
        //! Checks if the given file exists
        [[nodiscard]] bool exists() const;

        //! Returns the file size and throws exceptions if it does not exist or is a dir
        [[nodiscard]] uint64_t file_size() const;
        
        //! Returns the last modified time
        [[nodiscard]] uint64_t last_modified() const;
        
        //! Recursively creates the folders for this path
        bool create_folder() const;
        
        //! Check whether the given filename exists and is a folder
        [[nodiscard]] bool is_folder() const;
        
        //! Finds all files under the given path. Exception if the given
        //  path is not a folder.
        [[nodiscard]] std::set<file::Path> find_files(const std::string& filter_extension = "") const;
        
        //! Checks whether the given path is a regular file.
        [[nodiscard]] bool is_regular() const;
        
        //! Deletes the file if it exists and is a file - otherwise returns false.
        bool delete_file() const;
        
        //! Deletes the folder, if this file is a folder.
        bool delete_folder(bool recursively) const;
        
        //! Moves the file to given destination
        bool move_to(const file::Path& to);
        
        [[nodiscard]] Path replace_extension(std::string_view ext) const;
        [[nodiscard]] Path add_extension(std::string_view ext) const;
        [[nodiscard]] bool has_extension() const;
        [[nodiscard]] Path remove_extension(const std::string& = "") const;
        
        FilePtr fopen(const std::string& access_rights) const;
        std::vector<char> retrieve_data() const;
        std::string read_file() const;
        
        std::string_view extension() const;
        
        [[nodiscard]] file::Path absolute() const;
        
        explicit operator std::string() const { return str(); }
        std::string toStr() const { return Meta::toStr<std::string>(str()); }
        nlohmann::json to_json() const;
        static std::string class_name() { return "path"; }
        static file::Path fromStr(const std::string& str);
    };

    Path operator/( const Path& lhs, const Path& rhs );
    //Path operator+( const Path& lhs, const Path& rhs);
    
    std::string exec(const char* cmd);

    bool valid_extension(const file::Path&, const std::string& filter_extension);

    Path cwd();
    void cd(const file::Path&);

    struct UseNotOsSep {};

    template<char separator, typename Container>
    std::string make_path( Container&& parts) {
        std::stringstream ss;
        bool was_empty = true;
        size_t count{0u};
        for(const auto &[index, part] : enumerate(parts)) {
            if(part && *part != '\0') {
                if(not was_empty || count > 0 || (count == 0 && index > 0))
                    ss << separator;
                ss << part;
                was_empty = false;
                ++count;
            } else {
                was_empty = true;
            }
        }
        return ss.str();
    }

    template<typename Container = std::initializer_list<const char*>>
    inline std::string make_path( Container&& parts, UseNotOsSep) {
        return make_path<file::Path::not_os_sep()>(std::forward<Container>(parts));
    }
    template<typename Container = std::initializer_list<const char*>>
    inline std::string make_path( Container&& parts) {
        return make_path<file::Path::os_sep()>(std::forward<Container>(parts));
    }
    template<char separator = file::Path::os_sep(), typename... Args>
    inline std::string make_path(utils::StringLike auto&& first, Args... args) {
        std::array<const char*, sizeof...(Args) + 1> parts{first, args...};
        return make_path<separator>(parts);
    }
}

std::ostream& operator<<(std::ostream& os, const file::Path& p);
