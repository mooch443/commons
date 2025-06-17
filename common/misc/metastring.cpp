#include <commons.pc.h>
#include <cerrno>   // for ENODATA, ENOSR, ENOSTR, ETIME

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
