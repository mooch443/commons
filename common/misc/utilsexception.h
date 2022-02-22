#pragma once
#include <commons.pc.h>

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

	~UtilsException() throw();

	virtual const char * what() const throw();

private:
	std::string msg;
};




}

