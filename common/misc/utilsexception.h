#pragma once
#include <misc/format.h>

#ifdef __cpp_lib_source_location
#include <source_location>
namespace cmn {
using source_location = std::source_location;
}
#else
namespace cmn {

class source_location {
    const char* _file_name;
    const char* _function_name;
    int _line;

    constexpr source_location(const char* file, const char* func, int line) noexcept
        : _file_name(file)
        , _function_name(func)
        , _line(line)
    { }
    
public:
    constexpr source_location() noexcept = default;
    source_location(const source_location& other) = default;
    source_location(source_location&& other) = default;

    static source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        int line = __builtin_LINE()) noexcept
    {
        return source_location(file, func, line);
    }

    constexpr const char* file_name() const noexcept { return _file_name; }
    constexpr const char* function_name() const noexcept { return _function_name; }
    constexpr int line() const noexcept { return _line; }
};

}
#endif

namespace cmn {

class UtilsException : public std::exception {
public:

	UtilsException(const std::string& str);
	UtilsException(const char*fmt, ...);

	~UtilsException() throw();

	virtual const char * what() const throw();

private:
	std::string msg;
};

template <typename T> struct type_t {};
template <typename T> inline type_t<T> type{};

template< class T, typename... Args>
struct CustomException : T {
    CustomException(type_t<T>, const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : T(cmn::format<FormatterType::UNIX>(args...))
    {
    }
};


template<typename T, typename... Args>
CustomException(type_t<T>, Args... args) -> CustomException<T, Args...>;

/*template<typename T, typename... Args>
T CustomException(const Args& ...args, cmn::source_location info = cmn::source_location::current()) {
    return T(cmn::format<FormatterType::UNIX>(args...));
}*/

template<typename... Args>
class SoftException : public std::exception {
public:
    SoftException(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : msg(cmn::format<FormatterType::UNIX>(args...)), std::exception(msg.c_str())
    {
        
    }
    
    ~SoftException() throw() {
    }

    virtual const char * what() const throw() {
        return msg.c_str();
    }

private:
    std::string msg;
};


template<typename... Args>
SoftException(Args... args) -> SoftException<Args...>;

template<FormatterType formatter, typename... Args>
struct U_EXCEPTION : UtilsException {
    U_EXCEPTION(const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : UtilsException(cmn::format<formatter>(args...))
    {
    }
};

template<FormatterType formatter, char const *prefix, FormatColor color, typename... Args>
struct FormatColoredPrefix {
    FormatColoredPrefix(const Args& ...args, cmn::source_location info = cmn::source_location::current()) {
        std::string str =
            "["
                + cmn::console_color<color, formatter>(
                std::string(prefix) + " " + std::string(info.file_name()) + ":" + std::to_string(info.line()) + cmn::current_time_string())
          + "] "
          + format<formatter>(args...);
        
        printf("%s\n", str.c_str());
    }
};

static constexpr const char WARNING_PREFIX[] = "WARNING";
template<typename... Args>
struct FormatWarning : FormatColoredPrefix<FormatterType::UNIX, WARNING_PREFIX, FormatColor::YELLOW, Args...> {
    FormatWarning(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, WARNING_PREFIX, FormatColor::YELLOW, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatWarning(Args... args) -> FormatWarning<Args...>;

template<FormatterType formatter = FormatterType::UNIX, typename... Args>
U_EXCEPTION(const Args& ...args) -> U_EXCEPTION<formatter, Args...>;

static constexpr const char ERROR_PREFIX[] = "ERROR";
template<typename... Args>
struct FormatError : FormatColoredPrefix<FormatterType::UNIX, ERROR_PREFIX, FormatColor::RED, Args...> {
    FormatError(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, ERROR_PREFIX, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatError(Args... args) -> FormatError<Args...>;

static constexpr const char EXCEPTION_PREFIX[] = "EXCEPT";
template<typename... Args>
struct FormatExcept : FormatColoredPrefix<FormatterType::UNIX, EXCEPTION_PREFIX, FormatColor::RED, Args...> {
    FormatExcept(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, EXCEPTION_PREFIX, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatExcept(Args... args) -> FormatExcept<Args...>;

template<typename... Args>
void DebugHeader(const Args& ...args) {
    std::string str = format<FormatterType::UNIX>(args...);
    auto line = utils::repeat("-", str.length());
    print(line.c_str());
    print(args...);
    print(line.c_str());
}


template<typename... Args>
void DebugCallback(const Args& ...args) {
    DebugHeader<Args...>(args...);
}


}
