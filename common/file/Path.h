#pragma once
#include <commons.pc.h>

namespace file {
    using namespace cmn;

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
        GETTER(std::string, str)

        mutable struct Stat {
            std::optional<bool> exists, is_folder, is_regular;
            std::optional<std::string> absolute;
            timestamp_t assigned_at{ 0u };
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
        
        static char os_sep() noexcept;
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
        std::string toStr() const { return str(); }
        static std::string class_name() { return "path"; }
        static file::Path fromStr(const std::string& str);
    };

    Path operator/( const Path& lhs, const Path& rhs );
    Path operator+( const Path& lhs, const Path& rhs);
    
    std::string exec(const char* cmd);

    bool valid_extension(const file::Path&, const std::string& filter_extension);

    Path cwd();
    void cd(const file::Path&);
}

std::ostream& operator<<(std::ostream& os, const file::Path& p);
