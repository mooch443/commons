#include "PathArray.h"

#include <sstream>
#include <cctype>

namespace file {
std::string sanitize_filename(const std::string& s) {
    std::ostringstream os;
    for (char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.') {
            os << c;
        }
    }
    return os.str();
}

std::optional<file::Path> find_parent(const file::PathArray& pathArray) {
    auto& paths = pathArray.get_paths();
    
    if (paths.empty())
        return std::nullopt;  // Return an empty string if the PathArray is empty
    
    auto parent_path = paths[0].remove_filename();
    bool same_parent = std::all_of(paths.begin(), paths.end(),
        [&parent_path](const file::Path& p) {
            return p.remove_filename() == parent_path;
        });
    
    if (same_parent) {
        return parent_path;  // All paths share the same parent
    }
    
    return std::nullopt;
}

std::string find_basename(const file::PathArray& pathArray) {
    auto& paths = pathArray.get_paths();
    
    if (paths.empty())
        return "";  // Return an empty string if the PathArray is empty
    
    if (paths.size() == 1)
        return sanitize_filename((std::string)paths[0].remove_extension().filename());  // Single file, return its name
    
    auto parent_path = find_parent(pathArray);
    if (parent_path)
        return sanitize_filename((std::string)parent_path->filename());  // All paths share the same parent
    
    std::string common_prefix = (std::string)paths[0].remove_extension().filename();
    for (const auto& path : paths) {
        auto filename = path.remove_extension().filename();
        auto [it1, it2] = std::mismatch(common_prefix.begin(), common_prefix.end(), filename.begin(), filename.end());
        common_prefix.erase(it1, common_prefix.end());
    }
    
    if (!common_prefix.empty()) {
        return sanitize_filename(common_prefix);  // Return common prefix if exists
    }
    
    return "mixed_files";  // Default case
}
}
