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

template<cmn::meta_encoding_t::Class mode>
consteval inline uint8_t required_storage_channels() noexcept {
    if constexpr(is_in(mode, meta_encoding_t::gray, meta_encoding_t::r3g3b2)) {
        return 1;
    } else if constexpr(mode == meta_encoding_t::rgb8) {
        return 3;
    } else if constexpr(mode == meta_encoding_t::binary) {
        return 0;
    } else
        static_assert(static_cast<uint32_t>(mode) == std::numeric_limits<uint32_t>::max(), "Unknown storage mode.");
}

template<cmn::meta_encoding_t::Class mode>
consteval inline uint8_t required_image_channels() noexcept {
    if constexpr(is_in(mode, meta_encoding_t::rgb8, meta_encoding_t::r3g3b2)) {
        return 3;
    } else if constexpr(mode == meta_encoding_t::gray) {
        return 1;
    } else if constexpr(mode == meta_encoding_t::binary) {
        return 1;
    } else
        static_assert(static_cast<uint32_t>(mode) == std::numeric_limits<uint32_t>::max(), "Unknown image mode.");
}

constexpr inline uint8_t required_storage_channels(cmn::meta_encoding_t::Class mode) {
    switch (mode) {
        case meta_encoding_t::gray:
            return required_storage_channels<cmn::meta_encoding_t::gray>();
        case meta_encoding_t::r3g3b2:
            return required_storage_channels<cmn::meta_encoding_t::r3g3b2>();
        case meta_encoding_t::rgb8:
            return required_storage_channels<cmn::meta_encoding_t::rgb8>();
        case meta_encoding_t::binary:
            return required_storage_channels<cmn::meta_encoding_t::binary>();
            
        default:
            throw U_EXCEPTION("Unknown mode: ", mode.name());
    }
}

constexpr inline uint8_t required_image_channels(cmn::meta_encoding_t::Class mode) {
    switch (mode) {
        case meta_encoding_t::gray:
            return required_image_channels<cmn::meta_encoding_t::gray>();
        case meta_encoding_t::r3g3b2:
            return required_image_channels<cmn::meta_encoding_t::r3g3b2>();
        case meta_encoding_t::rgb8:
            return required_image_channels<cmn::meta_encoding_t::rgb8>();
        case meta_encoding_t::binary:
            return required_image_channels<cmn::meta_encoding_t::binary>();
            
        default:
            throw U_EXCEPTION("Unknown mode: ", mode.name());
    }
}

constexpr inline uint8_t required_storage_channels(cmn::ImageMode mode) {
    switch (mode) {
        case ImageMode::GRAY:
            return required_storage_channels<cmn::meta_encoding_t::gray>();
        case ImageMode::R3G3B2:
            return required_storage_channels<cmn::meta_encoding_t::r3g3b2>();
        case ImageMode::RGB:
            return required_storage_channels<cmn::meta_encoding_t::rgb8>();
        case ImageMode::RGBA:
            return 4;
            
        default:
            throw U_EXCEPTION("Unknown mode: ", (uint32_t)mode);
    }
}

constexpr inline uint8_t required_image_channels(cmn::ImageMode mode) {
    switch (mode) {
        case ImageMode::GRAY:
            return required_image_channels<cmn::meta_encoding_t::gray>();
        case ImageMode::R3G3B2:
            return required_image_channels<cmn::meta_encoding_t::r3g3b2>();
        case ImageMode::RGB:
            return required_image_channels<cmn::meta_encoding_t::rgb8>();
        case ImageMode::RGBA:
            return 4;
            
        default:
            throw U_EXCEPTION("Unknown mode: ", (uint32_t)mode);
    }
}

}
