#pragma once
#include <commons.pc.h>

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

