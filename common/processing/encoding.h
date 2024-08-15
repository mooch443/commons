#pragma once
#include <commons.pc.h>

namespace cmn {

ENUM_CLASS(DifferenceMethod_t, absolute, sign, none);
/*enum class DifferenceMethod {
    absolute,
    sign,
    none
};*/
using DifferenceMethod = DifferenceMethod_t::Class;

ENUM_CLASS(meta_encoding_t, gray, r3g3b2, rgb8);

}
