module;
#define IN_MODULE_INTERFACE 1

export module commons;

export {
#include "commons.exports.hpp"
}

#undef IN_MODULE_INTERFACE
