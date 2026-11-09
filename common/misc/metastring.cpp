#include <commons.pc.h>
#include <cerrno>   // for ENODATA, ENOSR, ENOSTR, ETIME

namespace {

// The payload is already unquoted. Limits count appended bytes, not code points.
template<bool limit_output>
void unescape_string(std::string& output, std::string_view sv, size_t max_output = 0) {
    if constexpr(limit_output) {
        output.reserve(output.size() + std::min(sv.size(), max_output));
    } else {
        output.reserve(output.size() + sv.size());
    }
    size_t pos = 0;
    [[maybe_unused]] std::conditional_t<limit_output, size_t, const void *> remaining;
    if constexpr(limit_output) {
        remaining = max_output;
    }

    const auto read_hex = [&](size_t offset) -> uint32_t {
        if(sv.size() - offset < 4)
            throw std::invalid_argument("Incomplete Unicode escape");
        uint32_t value = 0;
        for(size_t i = 0; i < 4; ++i) {
            const char c = sv[offset + i];
            uint32_t digit;
            if(c >= '0' && c <= '9') digit = c - '0';
            else if(c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if(c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else throw std::invalid_argument("Invalid Unicode escape");
            value = (value << 4) | digit;
        }
        return value;
    };

    while(pos < sv.size()) {
        if constexpr(limit_output) {
            if(remaining == 0)
                break;
        }
        
        size_t available;
        if constexpr(limit_output) {
            available = std::min(sv.size() - pos, remaining);
        } else {
            available = sv.size() - pos;
        }
        
        const auto bs = static_cast<const char*>(std::memchr(sv.data() + pos, '\\', available));
        if(not bs) {
            output.append(sv.data() + pos, available);
            break;
        }

        const size_t length = static_cast<size_t>(bs - (sv.data() + pos));
        output.append(sv.data() + pos, length);
        if constexpr(limit_output) {
            remaining -= length;
        }
        pos += length + 1;
        if(pos == sv.size()) {
            output.push_back('\\');
            break;
        }

        char decoded = sv[pos++];
        switch(decoded) {
            case 'b': decoded = '\b'; break;
            case 'f': decoded = '\f'; break;
            case 'n': decoded = '\n'; break;
            case 'r': decoded = '\r'; break;
            case 't': decoded = '\t'; break;
            case 'u': {
                uint32_t codepoint = read_hex(pos);
                pos += 4;
                if(codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if(sv.size() - pos < 6 || sv[pos] != '\\' || sv[pos + 1] != 'u')
                        throw std::invalid_argument("Missing low Unicode surrogate");
                    const uint32_t low = read_hex(pos + 2);
                    if(low < 0xDC00 || low > 0xDFFF)
                        throw std::invalid_argument("Invalid low Unicode surrogate");
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    pos += 6;
                } else if(codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                    throw std::invalid_argument("Unexpected low Unicode surrogate");
                }

                char bytes[4];
                size_t count;
                if(codepoint < 0x80) {
                    bytes[0] = static_cast<char>(codepoint);
                    count = 1;
                } else if(codepoint < 0x800) {
                    bytes[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                    bytes[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
                    count = 2;
                } else if(codepoint < 0x10000) {
                    bytes[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                    bytes[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    bytes[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
                    count = 3;
                } else {
                    bytes[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                    bytes[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    bytes[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    bytes[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
                    count = 4;
                }
                if constexpr(limit_output) {
                    count = std::min(count, remaining);
                    remaining -= count;
                }
                output.append(bytes, count);
                continue;
            }
            default: break; // Unknown escapes quote the following character literally.
        }
        output.push_back(decoded);
        if constexpr(limit_output) {
            --remaining;
        }
    }
}

}

namespace cmn::util {

std::string escape(std::string_view str) {
    std::string output;
    output.reserve(str.size());
    constexpr char hex[] = "0123456789abcdef";
    for(unsigned char c : str) {
        switch(c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if(c < 0x20) {
                    output += "\\u00";
                    output += hex[c >> 4];
                    output += hex[c & 0x0F];
                } else {
                    output += static_cast<char>(c);
                }
        }
    }
    return output;
}

std::string unescape(std::string_view str) {
    std::string output;
    unescape_string<false>(output, str);
    return output;
}

}

namespace cmn::utils {

void fast_fromstr_N(std::string& output, std::string_view sv, size_t max_output) {
    if(sv.size() >= 2 && is_in(sv.front(), '\'', '"') && sv.front() == sv.back()) {
        unescape_string<true>(output, sv.substr(1, sv.size() - 2), max_output);
    } else {
        output.append(sv.substr(0, max_output));
    }
}

void fast_fromstr(std::string& output, std::string_view sv) {
    if(sv.size() >= 2 && is_in(sv.front(), '\'', '"') && sv.front() == sv.back()) {
        unescape_string<false>(output, sv.substr(1, sv.size() - 2));
    } else {
        output.append(sv);
    }
}

}

namespace cmn::util {
const char* to_readable_errc(const std::errc& error_code) {
    switch (error_code) {
        case std::errc::address_family_not_supported: return "Address family not supported";
        case std::errc::address_in_use: return "Address in use";
        case std::errc::address_not_available: return "Address not available";
        case std::errc::already_connected: return "Socket is already connected";
        case std::errc::argument_list_too_long: return "Argument list too long";
        case std::errc::argument_out_of_domain: return "Mathematical argument out of domain";
        case std::errc::bad_address: return "Bad address";
        case std::errc::bad_file_descriptor: return "Bad file descriptor";
        case std::errc::bad_message: return "Bad message";
        case std::errc::broken_pipe: return "Broken pipe";
        case std::errc::connection_aborted: return "Connection aborted";
        case std::errc::connection_already_in_progress: return "Connection already in progress";
        case std::errc::connection_refused: return "Connection refused";
        case std::errc::connection_reset: return "Connection reset";
        case std::errc::cross_device_link: return "Cross-device link";
        case std::errc::destination_address_required: return "Destination address required";
        case std::errc::device_or_resource_busy: return "Device or resource busy";
        case std::errc::directory_not_empty: return "Directory not empty";
        case std::errc::executable_format_error: return "Executable format error";
        case std::errc::file_exists: return "File exists";
        case std::errc::file_too_large: return "File too large";
        case std::errc::filename_too_long: return "Filename too long";
        case std::errc::function_not_supported: return "Function not supported";
        case std::errc::host_unreachable: return "Host is unreachable";
        case std::errc::identifier_removed: return "Identifier removed";
        case std::errc::illegal_byte_sequence: return "Illegal byte sequence";
        case std::errc::inappropriate_io_control_operation: return "Inappropriate I/O control operation";
        case std::errc::interrupted: return "Interrupted system call";
        case std::errc::invalid_argument: return "Invalid argument";
        case std::errc::invalid_seek: return "Invalid seek";
        case std::errc::io_error: return "I/O error";
        case std::errc::is_a_directory: return "Is a directory";
        case std::errc::message_size: return "Message size";
        case std::errc::network_down: return "Network is down";
        case std::errc::network_reset: return "Network dropped connection on reset";
        case std::errc::network_unreachable: return "Network is unreachable";
        case std::errc::no_buffer_space: return "No buffer space available";
        case std::errc::no_child_process: return "No child processes";
        case std::errc::no_link: return "Link has been severed";
        case std::errc::no_lock_available: return "No locks available";
        // ------------------------------------------------------------------
        // POSIX‑STREAM error codes: present but [[deprecated]] since C++23.
        // We keep them only for pre‑C++23 builds; otherwise we match on the
        // underlying POSIX errno values (still valid, never deprecated).
        // ------------------------------------------------------------------
#if __cplusplus < 202302L       // up to C++20 → safe to use the names
        case std::errc::no_message_available: return "No message of desired type";
        case std::errc::stream_timeout:       return "Stream timeout";
        case std::errc::not_a_stream:         return "Not a stream";
        case std::errc::no_stream_resources:  return "No stream resources";
#else                           // C++23 or newer
#ifdef ENODATA
        case static_cast<std::errc>(ENODATA): return "No message of desired type";
#endif
#ifdef ENOSR
        case static_cast<std::errc>(ENOSR):   return "No stream resources";
#endif
#ifdef ENOSTR
        case static_cast<std::errc>(ENOSTR):  return "Not a stream";
#endif
#ifdef ETIME
        case static_cast<std::errc>(ETIME):   return "Stream timeout";
#endif
#endif
        case std::errc::no_protocol_option: return "Protocol not available";
        case std::errc::no_space_on_device: return "No space left on device";
        case std::errc::no_such_device_or_address: return "No such device or address";
        case std::errc::no_such_device: return "No such device";
        case std::errc::no_such_file_or_directory: return "No such file or directory";
        case std::errc::no_such_process: return "No such process";
        case std::errc::no_message: return "No message";
        case std::errc::not_a_directory: return "Not a directory";
        case std::errc::not_a_socket: return "Socket operation on non-socket";
        case std::errc::not_connected: return "Socket is not connected";
        case std::errc::not_enough_memory: return "Not enough space";
        case std::errc::not_supported: return "Operation not supported";
        case std::errc::operation_canceled: return "Operation canceled";
        case std::errc::operation_in_progress: return "Operation now in progress";
        case std::errc::operation_not_permitted: return "Operation not permitted";
        //case std::errc::operation_not_supported: return "Operation not supported on socket";
        case std::errc::operation_would_block: return "Operation would block";
        case std::errc::owner_dead: return "Owner dead";
        case std::errc::permission_denied: return "Permission denied";
        case std::errc::protocol_error: return "Protocol error";
        case std::errc::protocol_not_supported: return "Protocol not supported";
        case std::errc::read_only_file_system: return "Read-only file system";
        case std::errc::resource_deadlock_would_occur: return "Resource deadlock avoided";
        //case std::errc::resource_unavailable_try_again: return "Resource temporarily unavailable";
        case std::errc::result_out_of_range: return "Numerical result out of range";
        case std::errc::state_not_recoverable: return "State not recoverable";
        case std::errc::text_file_busy: return "Text file busy";
        case std::errc::timed_out: return "Connection timed out";
        case std::errc::too_many_files_open_in_system: return "Too many open files in system";
        case std::errc::too_many_files_open: return "Too many open files";
        case std::errc::too_many_links: return "Too many links";
        case std::errc::value_too_large: return "Value too large for defined data type";
        case std::errc::wrong_protocol_type: return "Protocol wrong type for socket";
        default: return "Unknown error";
    }
}

}
