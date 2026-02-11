#pragma once
#if defined(__APPLE__)

#include <string>
#include <optional>
#include <expected>

namespace cmn::file {

/**
 * @brief Ask the system to grant (or deny) access to a file/folder.
 *
 * The function resolves a security‑scoped bookmark for the given path
 * **in a background thread** and waits until the user answers the
 * system dialog.  The caller can simply block on this function; it
 * returns `true` if access was granted, or `false` otherwise.
 *
 * @param path  Absolute POSIX path to the resource (file or folder)
 * @return std::pair<bool, std::optional<std::string>>
 *         - first  : true = access granted
 *         - second : error message if any (empty on success)
 */
std::expected<void, std::string> request_access(const std::string& path);

}

#else
namespace cmn::file {
inline std::expected<void, std::string> request_access(const std::string&) {
    return {};
}
}
#endif