#pragma once

#include <commons.pc.h>
#include <file/Path.h>

//#define COMMON_DEBUG_PATH_RESOLVE

namespace cmn::file {

/**
 * @class PathArray
 * @brief Handles an array of file paths.
 *
 * This class manages an array of file::Path objects. It can initialize the array
 * from a std::string or a file::Path object, and can handle path patterns and placeholders.
 */
template <typename FS = RealFilesystem>
class _PathArray {
public:
    struct DeferredFileCheck {
        std::string path;
        int start;
        int end;
        int padding;
    };

    static inline const std::regex pattern{ R"(%0?(\d+)\.(\d+)\.(\d+)d|%0?(\d+)\.(\d+)d|%0?(\d+)d|\*)" };

private:
    GETTER_SETTER(std::string, source);
    GETTER_I(bool, matched_patterns, false);

    mutable bool _has_to_be_filtered{ false };
    mutable std::optional<std::vector<std::string>> _to_be_resolved;
    mutable std::vector<file::Path> _paths; ///< Member to store the list of file paths
    mutable std::vector<DeferredFileCheck> _deferredFileChecks;

    FS fs; // Object to interact with the filesystem
    
    /**
     * @brief Add a file::Path to _paths if it's not empty.
     *
     * @param path The file::Path to add.
     */
    void add_path_if_not_empty(const file::Path& path) {
        if (!path.empty()) {
            _paths.push_back(path);
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Added path to _paths: ", path.str());
#endif
        } else
            FormatWarning("Not adding empty path: ", path);
    }

public:
    static void ensure_loaded_static(std::vector<file::Path>& paths,
        std::vector<DeferredFileCheck>& deferredFileChecks,
        std::optional<std::vector<std::string>>& to_be_resolved,
        bool& has_to_be_filtered,
        const FS& fs)
    {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("ensure_loaded_static() called");
        Print("Current paths size: ", paths.size());
        if (!deferredFileChecks.empty())
            Print("There are deferred file checks to process: ", deferredFileChecks.size());
        if (to_be_resolved.has_value())
            Print("There are paths to be resolved: ", to_be_resolved->size());
#endif

        if (not deferredFileChecks.empty()) {
            std::ostringstream ss;
            for (const auto& dfc : deferredFileChecks) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                Print("Processing DeferredFileCheck: path=", dfc.path, ", start=", dfc.start, ", padding=", dfc.padding);
#endif
                for (int i = dfc.start; ; ++i) {
                    ss.str("");
                    ss << std::setw(dfc.padding) << std::setfill('0') << i;
                    auto replaced_path = std::regex_replace(dfc.path, pattern, ss.str());
                    file::Path filePath(replaced_path);
                    if (not fs.exists(filePath)) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                        Print("File does not exist, breaking: ", filePath.str());
#endif
                        break;
                    }
#ifdef COMMON_DEBUG_PATH_RESOLVE
                    Print("Deferred check added existing path: ", filePath.str());
#endif
                    paths.push_back(filePath);
                }
            }
            deferredFileChecks.clear();
        }

        if (to_be_resolved.has_value() && not to_be_resolved->empty()) {
            std::string path = to_be_resolved.value().front();
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Resolving paths for pattern: ", path);
#endif
            file::Path parent_path = file::Path(path).remove_filename();
            if (fs.is_folder(parent_path)) {
                auto all_files = fs.find_files(parent_path);

                // Replace * with .* and escape other special characters
                std::string regex_str = "^" + escape_regex(path) + "$";
                std::regex star_replace("\\*");
                regex_str = std::regex_replace(regex_str, star_replace, ".*");
                try {
                    std::regex file_matcher(regex_str);

                    for (const auto& file : all_files) {
                        if (std::regex_match(file.str(), file_matcher)) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                            Print("Pattern matched file: ", file.str());
#endif
                            paths.push_back(file);
                        }
                    }
                }
                catch (const std::regex_error& e) {
                    // Invalid regex, treat it as a regular path
                    paths.push_back(file::Path(path));
                    FormatWarning("Cannot parse regex: ", regex_str, " (", e.what(), ")");
                }
                
                std::sort(paths.begin(), paths.end());
            }

            has_to_be_filtered = true;
            to_be_resolved.reset();
        }

        if (has_to_be_filtered) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Filtering paths for existence. Paths count before filtering: ", paths.size());
#endif
            std::vector<file::Path> existing_paths;
            std::copy_if(paths.begin(), paths.end(), std::back_inserter(existing_paths), [&fs](const file::Path& p) { return fs.exists(p); });
            paths = std::move(existing_paths);
            has_to_be_filtered = false;
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Paths count after filtering: ", paths.size());
#endif
        }
    }
    
public:
    void ensure_loaded() const {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("ensure_loaded() called");
#endif
        ensure_loaded_static(_paths, _deferredFileChecks, _to_be_resolved, _has_to_be_filtered, fs);
    }

public:
    bool has_to_be_resolved() const { return _to_be_resolved.has_value(); }

    /**
     * @brief Constructor for initializing with a std::string.
     *
     * @param input The input std::string. It can be a path pattern, a single path, or an array of paths.
     */
    _PathArray() = default;
    _PathArray(const _PathArray& other)
        : _source(other._source),
          _matched_patterns(other._matched_patterns),
          _has_to_be_filtered(other._has_to_be_filtered),
          _to_be_resolved(other._to_be_resolved),
          _paths(other._paths),
          _deferredFileChecks(other._deferredFileChecks),
          fs(other.fs)
    { }
    _PathArray(_PathArray&& other)
        : _source(std::move(other._source)),
          _matched_patterns(std::move(other._matched_patterns)),
          _has_to_be_filtered(std::move(other._has_to_be_filtered)),
          _to_be_resolved(std::move(other._to_be_resolved)),
          _paths(std::move(other._paths)),
          _deferredFileChecks(std::move(other._deferredFileChecks)),
          fs(std::move(other.fs))
    { }
    _PathArray& operator=(const _PathArray& other) {
        _source = other.source();
        _paths = other._paths;
        _matched_patterns = other._matched_patterns;
        _to_be_resolved = other._to_be_resolved;
        _deferredFileChecks = other._deferredFileChecks;
        _has_to_be_filtered = other._has_to_be_filtered;
        fs = other.fs;
        return *this;
    }
    _PathArray& operator=(_PathArray&& other) {
        _source = std::move(other._source);
        _paths = std::move(other._paths);
        _matched_patterns = std::move(other._matched_patterns);
        _to_be_resolved = std::move(other._to_be_resolved);
        _deferredFileChecks = std::move(other._deferredFileChecks);
        _has_to_be_filtered = std::move(other._has_to_be_filtered);
        fs = std::move(other.fs);
        return *this;
    }
    
    template<utils::StringLike T>
    _PathArray(T&& input)
        : _source(input)
    {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("Constructing PathArray from input: ", input);
#endif
        std::string_view sv;
        
        if constexpr(std::is_same_v<std::remove_cvref_t<T>, const char*>) {
            if(not input || *input == 0)
                return;
            
            sv = { input };
            
        } else if constexpr(std::is_array_v<std::remove_cvref_t<T>>) {
            if constexpr(sizeof(T) == 0u) {
                return;
            }
            
            sv = { reinterpret_cast<const char*>(input), sizeof(T) - 1 };
            
        } else {
            if (input.empty())
                return;
            
            sv = { input };
        }
        
        if(utils::beginsWith(sv, "PathArray<")
           && sv.back() == '>')
        {
            throw InvalidArgumentException("Cannot instantiate with an unresolved PathArray: ", sv);
        }
        
        // Check if input is an array of paths
        if (sv.front() == '[' && sv.back() == ']') {
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Input appears to be a JSON-like array of paths.");
#endif
            std::vector<std::string> temp_paths = Meta::fromStr<std::vector<std::string>>((std::string)sv);
            for (const auto& path_str : temp_paths) {
                add_path(path_str);
            }
        } else {
            add_path((std::string)sv);
        }
        
        assert(not _paths.empty() || sv.empty() || _matched_patterns);
        if(_paths.size() <= 1 && not matched_patterns()) {
            if(not _paths.empty())
                _source = _paths.front().str();
        }
    }
    
    explicit _PathArray(const file::Path& input)
        : _PathArray(input.str())
    { }

    _PathArray(const std::vector<std::string>& paths) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("Constructing PathArray from vector of strings. Count: ", paths.size());
#endif
        for(auto &path : paths) {
            add_path(path);
        }

        if(_paths.size() == 1) {
            _source = _paths.front().str();
        } else
            _source = Meta::toStr(_paths);

        assert(not _paths.empty() || _source.empty());
    }
    
    /**
    * @brief Add a path using its string representation.
    *
    * @param path The path string to add.
    */
    void add_path(const std::string& path) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("add_path called with: ", path);
#endif
        auto parsed_paths = parse_path(path, _matched_patterns, _to_be_resolved, _deferredFileChecks, _has_to_be_filtered);
        for (const auto& parsed_path : parsed_paths) {
          add_path_if_not_empty(parsed_path);
        }
    }
    
    /* _PathArray(const std::vector<file::Path>& paths) : _paths(paths) {
        if(paths.size() == 1) {
            _source = paths.front().str();
        } else
            _source = Meta::toStr(paths);
    }*/
    
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
        /*if (auto cmp = _paths.size() <=> other._paths.size(); cmp != 0) {
            return cmp;
        }

        // Sizes are equal, compare the elements lexicographically
        for (auto it1 = _paths.begin(), it2 = other._paths.begin(); it1 != _paths.end(); ++it1, ++it2) {
            if (auto cmp = *it1 <=> *it2; cmp != 0) {
                return cmp;
            }
        }

        return std::strong_ordering::equal;*/
        return source() <=> other.source();
    }
    
    bool operator==(const _PathArray& other) const {
        return other.source()  == source();
    }
    bool operator!=(const _PathArray& other) const {
        return other.source() != source();
    }
    
    static _PathArray fromStr(const std::string& str) {
        return { str };
    }
    std::string blocking_toStr() const {
        ensure_loaded();
        if(_matched_patterns && _paths.empty())
            return _source;
        else if(_paths.empty())
            return "\"\"";
        if(_paths.size() == 1)
            return Meta::toStr(_paths.front().str());
        return Meta::toStr(_paths);
    }
    std::string toStr() const {
        if (_to_be_resolved.has_value()) {
            return "PathArray<to be resolved:" + source() + ">";
        }

        if(_matched_patterns && _paths.empty())
            return _source;
        else if(_paths.empty())
            return "\"\"";
        if(_paths.size() == 1)
            return Meta::toStr(_paths.front().str());
        return Meta::toStr(_paths);
    }
    glz::json_t to_json() const {
        return glz::json_t(source());
    }
    static std::string class_name() {
        return "PathArray";
    }
    
    static std::string escape_regex(const std::string& str) {
        // List of regex special characters that need to be escaped, including the backslash
        static const std::regex esc("[.^$|()\\[\\]{}+?\\\\]");
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
    template<typename T>
    static std::vector<file::Path> parse_path(const std::string& path, bool& matched_patterns, T&& to_be_resolved, auto& deferredFileChecks, bool& has_to_be_filtered) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
        Print("parse_path called with: ", path);
#endif
        std::smatch match;
        std::vector<file::Path> parsed_paths;
        FS fs;

        if (std::regex_search(path, match, pattern)) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("Path contains a pattern");
#endif
            matched_patterns = true;
            std::ostringstream ss;
            
            // Handle cases like %0.100.6d
            if (match[1].length() && match[2].length() && match[3].length()) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                Print("Detected pattern %0.<end>.<pad>d with start=", match[1], ", end=", match[2], ", padding=", match[3]);
#endif
                int start = std::stoi(match[1]);
                int end = std::stoi(match[2]);
                int padding = std::stoi(match[3]);
                
                // Iterate over the range and apply padding
                for (int i = start; i <= end; ++i) {
                    ss.str("");
                    ss << std::setw(padding) << std::setfill('0') << i;
                    auto replaced_path = std::regex_replace(path, pattern, ss.str());
                    file::Path filePath(replaced_path);
#ifdef COMMON_DEBUG_PATH_RESOLVE
                    Print("Generated path from pattern: ", filePath.str());
#endif
                    parsed_paths.push_back(filePath);
                }

                // filter for existing files later on:
                has_to_be_filtered = true;
            }
            // Handle cases like %10.6d
            else if (match[4].length() && match[5].length()) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                Print("Detected pattern %<start>.<pad>d with start=", match[4], ", padding=", match[5]);
#endif
                int padding = std::stoi(match[5]);
                int starting_index = std::stoi(match[4]);
                deferredFileChecks.push_back({ path, starting_index, -1, padding });
            }
            // Handle cases like %6d
            else if (match[6].length()) {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                Print("Detected pattern %<pad>d with padding=", match[6]);
#endif
                int padding = std::stoi(match[6]);
                deferredFileChecks.push_back({ path, 0, -1, padding });
                
            } else if (match.str() == "*") {
#ifdef COMMON_DEBUG_PATH_RESOLVE
                Print("Detected wildcard pattern *");
#endif
                to_be_resolved = std::vector<std::string>{ path };
            }
            
        } else {
            // No pattern found, treat it as a regular path
#ifdef COMMON_DEBUG_PATH_RESOLVE
            Print("No pattern found, regular path: ", path);
#endif
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
        ensure_loaded();
        return _paths.empty();
    }
    
    /**
     * @brief Get the beginning iterator for _paths.
     *
     * @return An iterator to the beginning of _paths.
     */
    auto begin() const -> decltype(_paths.begin()) {
        ensure_loaded();
        return _paths.begin();
    }
    
    /**
     * @brief Get the ending iterator for _paths.
     *
     * @return An iterator to the end of _paths.
     */
    auto end() const -> decltype(_paths.end()) {
        ensure_loaded();
        return _paths.end();
    }
    
    [[nodiscard]] size_t size() const {
        ensure_loaded();
        return _paths.size();
    }
    
    /**
     * @brief Get the paths stored in _paths.
     *
     * @return A constant reference to _paths.
     */
    [[nodiscard]] const std::vector<file::Path>& get_paths() const {
        ensure_loaded();
        return _paths;
    }
    
    /**
     * @brief Access the file::Path at the given index.
     *
     * @param index The index of the path to access.
     * @return A constant reference to the file::Path object at the specified index.
     */
    const file::Path& operator[](std::size_t index) const {
        ensure_loaded();
        if (index >= _paths.size()) {
            throw std::out_of_range("Index out of range");
        }
        return _paths[index];
    }

};


// Alias for production code
using PathArray = _PathArray<>;

namespace detail {
    [[nodiscard]] inline constexpr bool is_ascii_alnum(char c) noexcept
    {
        return  ('0' <= c && c <= '9') ||
            ('A' <= c && c <= 'Z') ||
            ('a' <= c && c <= 'z');
    }

    [[nodiscard]] inline constexpr bool is_allowed(char c) noexcept
    {
        return is_ascii_alnum(c) || c == '-' || c == '_' || c == '.';
    }
}

template<utils::StringLike Str>
[[nodiscard]] constexpr inline std::string sanitize_filename(Str&& in)
{
    std::string_view src{ std::forward<Str>(in) };
    std::string      out;
    out.reserve(src.size());        // <= 1 allocation (ignored in const-expr)

    bool have_output = false;     // have we emitted any non-space yet?
    bool pending_space = false;     // one space waiting to be flushed

    for (char c : src)
    {
        // drop everything we never allow
        if (!(detail::is_allowed(c) || c == ' '))
            continue;

        if (c == ' ')
        {
            if (!have_output)       // still in leading spaces: skip
                continue;
            pending_space = true;   // might become an inner space
            continue;
        }

        // flush exactly one queued space (if it was inner)
        if (pending_space)
        {
            out.push_back(' ');
            pending_space = false;
        }
        out.push_back(c);
        have_output = true;
    }
    // any space still pending here was trailing -> discard it
    return out;
}

std::string find_basename(const file::PathArray& pathArray);
std::optional<file::Path> find_parent(const file::PathArray& pathArray);

}
