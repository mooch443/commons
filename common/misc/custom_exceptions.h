#pragma once
#include <commons.pc.h>

namespace cmn {

class SoftExceptionImpl : public std::exception {
public:
    template<typename... Args>
    SoftExceptionImpl(cmn::source_location info, const Args& ...args)
    : msg(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
    
    ~SoftExceptionImpl() throw() {
    }
    
    virtual const char * what() const throw() {
        return msg.c_str();
    }
    
private:
    std::string msg;
};

template< typename... Args>
struct _SoftException : SoftExceptionImpl {
    _SoftException(cmn::source_location info, const Args& ...args) noexcept(false)
    : SoftExceptionImpl(info, args...)
    { }
};

template<typename... Args>
_SoftException(cmn::source_location loc, const Args& ...args) -> _SoftException<Args...>;

#define SoftException(...) _SoftException(cmn::source_location::current(), __VA_ARGS__)

template <typename T> struct type_t {};
template <typename T> inline type_t<T> type{};

template< class T, typename... Args>
struct CustomException : T {
    CustomException(type_t<T>, const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
    : T(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
};


template<typename T, typename... Args>
CustomException(type_t<T>, Args... args) -> CustomException<T, Args...>;


template<typename... Args>
struct InvalidArgumentException : CustomException<std::invalid_argument, Args...> {
    InvalidArgumentException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
    : CustomException<std::invalid_argument, Args...>(type_t<std::invalid_argument>{}, args..., info)
    {
    }
};

template<typename... Args>
InvalidArgumentException(Args... args) -> InvalidArgumentException<Args...>;

template<typename... Args>
struct InvalidSyntaxException : CustomException<std::runtime_error, Args...> {
    InvalidSyntaxException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
    : CustomException<std::runtime_error, Args...>(type_t<std::runtime_error>{}, args..., info)
    {
    }
};

template<typename... Args>
InvalidSyntaxException(Args... args) -> InvalidSyntaxException<Args...>;

template<typename... Args>
struct OutOfRangeException : CustomException<std::out_of_range, Args...> {
    OutOfRangeException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
    : CustomException<std::out_of_range, Args...>(type_t<std::out_of_range>{}, args..., info)
    {
    }
};

template<typename... Args>
OutOfRangeException(Args... args) -> OutOfRangeException<Args...>;

namespace Meta {
template<> inline std::string name<SoftExceptionImpl>() {
    return "SoftException";
}

template<> inline std::string name<UtilsException>() {
    return "UtilsException";
}
}

}
