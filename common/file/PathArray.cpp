#include "PathArray.h"

#include <sstream>
#include <cctype>

namespace cmn::file {
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
    constexpr auto remove_if_common = [](file::Path path) {
        static constexpr std::array common_extensions {
            "pv",
            "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "m2v", "m4v", "3gp", "3g2", "asf", "rm", "swf", "vob", "ts", "m2ts", "mts", "divx", "xvid", "rmvb", "f4v", "ogv", "drc", "gifv", "mng", "mp2", "qt", "yuv", "txt", "json", "xml", "html", "htm", "xhtml", "css", "js", "c", "cpp", "h", "hpp", "py", "java", "class", "cs", "sh", "bat", "cmd", "ps1", "vbs", "vb", "go", "pl", "rb", "lua", "ini", "cfg", "reg", "csv", "tsv", "dat", "db", "sql", "mdb", "log", "tar", "zip", "rar", "gz", "7z", "iso", "bin", "exe", "dll", "msi", "apk", "jar", "img", "dmg", "pdf", "epub", "mobi", "azw", "azw3", "azw4", "prc", "azw6", "azw1", "txtz", "txt", "rtf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "pps", "ppsx", "odt", "ods", "odp", "odg", "odf", "rtf", "ps", "tex", "wks", "wps", "wpd", "key", "numbers", "pages", "jpg", "jpeg", "png", "gif", "bmp", "svg", "ico", "tif", "tiff", "webp", "heic", "heif", "mp3", "wav", "wma", "flac", "aac", "ogg", "opus", "m4a", "m4b", "m4p", "m4r", "3ga", "aif", "aiff", "aifc", "ape", "au", "amr", "awb", "dct", "dss", "dvf", "flp", "gsm", "iklax", "ivs"
        };
        if(path.has_extension()
           && contains(common_extensions, utils::lowercase(path.extension())))
        {
            return path.remove_extension();
        }
        
        return path;
    };
    
    auto& paths = pathArray.get_paths();
    assert(pathArray.matched_patterns() || pathArray.source().empty() || not paths.empty());
    
    if (paths.empty())
        return "";  // Return an empty string if the PathArray is empty
    
    if (paths.size() == 1)
        return sanitize_filename((std::string)remove_if_common(paths[0]).filename());  // Single file, return its name
    
    auto parent_path = find_parent(pathArray);
    if (parent_path)
        return sanitize_filename((std::string)parent_path->filename());  // All paths share the same parent
    
    std::string common_prefix = (std::string)remove_if_common(paths[0]).filename();
    for (const auto& path : paths) {
        auto filename = remove_if_common(path).filename();
        auto [it1, it2] = std::mismatch(common_prefix.begin(), common_prefix.end(), filename.begin(), filename.end());
        common_prefix.erase(it1, common_prefix.end());
    }
    
    if (!common_prefix.empty()) {
        return sanitize_filename(common_prefix);  // Return common prefix if exists
    }
    
    return "mixed_files";  // Default case
}

}
