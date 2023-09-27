#pragma once

#include <commons.pc.h>
#include <file/Path.h>
#include <misc/metastring.h>
#include <regex>

namespace file {

// FilesystemInterface that both real and mock classes should implement
struct FilesystemInterface {
    virtual std::set<file::Path> find_files(const file::Path&) const = 0;
    virtual bool is_folder(const file::Path&) const = 0;
    virtual bool exists(const file::Path&) const = 0;
    virtual ~FilesystemInterface() = default;
};

// RealFilesystem that uses the actual filesystem calls
struct RealFilesystem : FilesystemInterface {
    std::set<file::Path> find_files(const file::Path& path) const override {
        return path.find_files(); // Actual implementation
    }

    bool is_folder(const file::Path& path) const override {
        return path.is_folder(); // Actual implementation
    }
    
    bool exists(const file::Path& path) const override {
        return path.exists(); // Actual implementation
    }
};

/**
 * @class PathArray
 * @brief Handles an array of file paths.
 *
 * This class manages an array of file::Path objects. It can initialize the array
 * from a std::string or a file::Path object, and can handle path patterns and placeholders.
 */
template <typename FS = RealFilesystem>
class _PathArray {
private:
    GETTER(std::string, source)
    
    std::vector<file::Path> _paths; ///< Member to store the list of file paths
    FS fs; // Object to interact with the filesystem
    
    /**
     * @brief Add a file::Path to _paths if it's not empty.
     *
     * @param path The file::Path to add.
     */
    void add_path_if_not_empty(const file::Path& path) {
        if (!path.empty()) {
            _paths.push_back(path);
        }
    }
    
public:
    /**
     * @brief Constructor for initializing with a std::string.
     *
     * @param input The input std::string. It can be a path pattern, a single path, or an array of paths.
     */
    _PathArray(const std::string& input = "")
        : _source(input)
    {
        if (input.empty()) return;
        
        // Check if input is an array of paths
        if (input.front() == '[' && input.back() == ']') {
            std::vector<std::string> temp_paths = Meta::fromStr<std::vector<std::string>>(input);
            for (const auto& path_str : temp_paths) {
                add_path(path_str);
            }
        } else {
            add_path(input);
        }
    }
    
    /**
    * @brief Add a path using its string representation.
    *
    * @param path The path string to add.
    */
    void add_path(const std::string& path) {
        auto parsed_paths = parse_path(path);
        for (const auto& parsed_path : parsed_paths) {
          add_path_if_not_empty(parsed_path);
        }
    }
    
    _PathArray(const std::vector<file::Path>& paths) : _source(Meta::toStr(paths)), _paths(paths) {}
    
    /**
     * @brief Three-way comparison operator (spaceship operator).
     *
     * Compares the two _PathArray objects lexicographically.
     *
     * @param other The other _PathArray to compare with.
     * @return A value of type std::strong_ordering.
     */
    auto operator<=>(const _PathArray& other) const {
        // First, you can check the sizes for quick comparison
        if (auto cmp = _paths.size() <=> other._paths.size(); cmp != 0) {
            return cmp;
        }

        // Sizes are equal, compare the elements lexicographically
        for (auto it1 = _paths.begin(), it2 = other._paths.begin(); it1 != _paths.end(); ++it1, ++it2) {
            if (auto cmp = *it1 <=> *it2; cmp != 0) {
                return cmp;
            }
        }

        return std::strong_ordering::equal;
    }
    
    bool operator==(const _PathArray& other) const {
        return other._paths == _paths;
    }
    
    static _PathArray fromStr(const std::string& str) {
        return { str };
    }
    std::string toStr() const {
        if(_paths.empty())
            return "";
        if(_paths.size() == 1)
            return _paths.front().str();
        return Meta::toStr(_paths);
    }
    static std::string class_name() {
        return "PathArray";
    }
    
    static std::string escape_regex(const std::string& str) {
        // List of regex special characters that need to be escaped
        static const std::regex esc("[.^$|()\\[\\]{}+?]");
        return std::regex_replace(str, esc, "\\$&");
    }
    
    /**
    * @brief Parse the pattern and return an array of file::Path.
    *
    * This function identifies different types of patterns in the input path,
    * such as %0.100.6d, %0.6d, %6d, and *. Based on these patterns,
    * it finds and returns all file::Path objects that match.
    *
    * @param path The path pattern to parse.
    * @return A vector of file::Path objects that match the pattern.
    */
    static std::vector<file::Path> parse_path(const std::string& path) {
        // Define the regex pattern for different types of placeholders in the path
        std::regex pattern(R"(%0?(\d+)\.(\d+)\.(\d+)d|%0?(\d+)\.(\d+)d|%0?(\d+)d|\*)");
        std::smatch match;
        std::vector<file::Path> parsed_paths;
        FS fs;

        if (std::regex_search(path, match, pattern)) {
            std::ostringstream ss;
            
            // Handle cases like %0.100.6d
            if (match[1].length() && match[2].length() && match[3].length()) {
                int start = std::stoi(match[1]);
                int end = std::stoi(match[2]);
                int padding = std::stoi(match[3]);
                
                // Iterate over the range and apply padding
                for (int i = start; i <= end; ++i) {
                    ss.str("");
                    ss << std::setw(padding) << std::setfill('0') << i;
                    auto replaced_path = std::regex_replace(path, pattern, ss.str());
                    file::Path filePath(replaced_path);
                    if (not fs.exists(filePath)) {
                        break;
                    }
                    parsed_paths.push_back(filePath);
                }
            }
            // Handle cases like %10.6d
            else if (match[4].length() && match[5].length()) {
                int padding = std::stoi(match[5]);
                int starting_index = std::stoi(match[4]);  // Parse the starting index
                
                // Iterate indefinitely until no matching files are found
                for (int i = starting_index; ; ++i) {  // Start from the parsed index
                    ss.str("");
                    ss << std::setw(padding) << std::setfill('0') << i;
                    auto replaced_path = std::regex_replace(path, pattern, ss.str());
                    file::Path filePath(replaced_path);
                    if (not fs.exists(filePath)) {
                        break;
                    }
                    parsed_paths.push_back(filePath);
                }
            }
            // Handle cases like %6d
            else if (match[6].length()) {
                int padding = std::stoi(match[6]);
                
                // Iterate indefinitely until no matching files are found
                for (int i = 0; ; ++i) {
                    ss.str("");
                    ss << std::setw(padding) << std::setfill('0') << i;
                    auto replaced_path = std::regex_replace(path, pattern, ss.str());
                    file::Path filePath(replaced_path);
                    if (not fs.exists(filePath)) {
                        break;
                    }
                    parsed_paths.push_back(filePath);
                }
                
            } else if (match.str() == "*") {
                // Handle cases like *
                file::Path parent_path = file::Path(path).remove_filename();
                if (fs.is_folder(parent_path)) {
                    auto all_files = fs.find_files(parent_path);
                    
                    // Replace * with .* and escape other special characters
                    std::string regex_str = "^" + escape_regex(path) + "$";
                    std::regex star_replace("\\*");
                    regex_str = std::regex_replace(regex_str, star_replace, ".*");
                    std::regex file_matcher(regex_str);
                    
                    for (const auto& file : all_files) {
                        if (std::regex_match(file.str(), file_matcher)) {
                            parsed_paths.push_back(file);
                        }
                    }
                }
            }
            
        } else {
            // No pattern found, treat it as a regular path
            parsed_paths.push_back(file::Path(path));
        }

        return parsed_paths;
    }
    
    /**
     * @brief Check if _paths is empty.
     *
     * @return True if empty, otherwise False.
     */
    [[nodiscard]] bool empty() const {
        return _paths.empty();
    }
    
    /**
     * @brief Get the beginning iterator for _paths.
     *
     * @return An iterator to the beginning of _paths.
     */
    auto begin() const -> decltype(_paths.begin()) {
        return _paths.begin();
    }
    
    /**
     * @brief Get the ending iterator for _paths.
     *
     * @return An iterator to the end of _paths.
     */
    auto end() const -> decltype(_paths.end()) {
        return _paths.end();
    }
    
    [[nodiscard]] size_t size() const {
        return _paths.size();
    }
    
    /**
     * @brief Get the paths stored in _paths.
     *
     * @return A constant reference to _paths.
     */
    [[nodiscard]] const std::vector<file::Path>& get_paths() const {
        return _paths;
    }
    
    /**
     * @brief Access the file::Path at the given index.
     *
     * @param index The index of the path to access.
     * @return A constant reference to the file::Path object at the specified index.
     */
    const file::Path& operator[](std::size_t index) const {
        if (index >= _paths.size()) {
            throw std::out_of_range("Index out of range");
        }
        return _paths[index];
    }

};


// Alias for production code
using PathArray = _PathArray<>;

std::string sanitize_filename(const std::string& s);
std::string find_basename(const file::PathArray& pathArray);

}
