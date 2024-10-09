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

ENUM_CLASS(meta_encoding_t, gray, r3g3b2, rgb8, binary);

constexpr inline uint8_t required_storage_channels(cmn::meta_encoding_t::Class mode) {
    switch (mode) {
        case meta_encoding_t::gray:
        case meta_encoding_t::r3g3b2:
            return 1;
        case meta_encoding_t::rgb8:
            return 3;
        case meta_encoding_t::binary:
            return 0;
            
        default:
            throw U_EXCEPTION("Unknown mode: ", mode.name());
    }
}

constexpr inline uint8_t required_image_channels(cmn::meta_encoding_t::Class mode) {
    switch (mode) {
        case meta_encoding_t::gray:
        case meta_encoding_t::r3g3b2:
            return 1;
        case meta_encoding_t::rgb8:
            return 3;
        case meta_encoding_t::binary:
            return 1;
            
        default:
            throw U_EXCEPTION("Unknown mode: ", mode.name());
    }
}

}
