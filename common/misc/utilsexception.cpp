#include "utilsexception.h"

namespace cmn {

UtilsException::UtilsException(const std::string& str) : std::exception() {
	msg = str;
}

UtilsException::~UtilsException() throw() {
}

const char * UtilsException::what() const throw() {
	return msg.c_str();
}

}
