#pragma once
#include <commons.pc.h>
#include <misc/FormatColor.h>

namespace cmn {

class UtilsException : public std::exception {
public:

	UtilsException(const std::string& str);

	~UtilsException() throw();

	virtual const char * what() const throw();

private:
	std::string msg;
};

template<FormatterType formatter, typename... Args>
struct _U_EXCEPTION : UtilsException {
    _U_EXCEPTION(cmn::source_location info, const Args& ...args) noexcept(false)
    : UtilsException(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<formatter, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
};

template<FormatterType formatter = FormatterType::UNIX, typename... Args>
_U_EXCEPTION(cmn::source_location loc, const Args& ...args) -> _U_EXCEPTION<formatter, Args...>;

#define U_EXCEPTION(...) _U_EXCEPTION(cmn::source_location::current(), __VA_ARGS__)


}

