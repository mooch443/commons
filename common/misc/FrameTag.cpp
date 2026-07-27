#include "FrameTag.h"

namespace cmn {

std::string IdentifiedTag::toStr() const {
    return "["+Meta::toStr(fdx)+","+Meta::toStr(bds)+","+Meta::toStr(name)+"]";
}

glz::json_t IdentifiedTag::to_json() const {
    return cvt2json(std::make_tuple(fdx, bds, name));
}

std::string SpatialTag::toStr() const {
    return "["+Meta::toStr(bds)+","+Meta::toStr(name)+"]";
}

glz::json_t SpatialTag::to_json() const {
    return cvt2json(std::make_tuple(bds, name));
}

void FrameTag::validate_name(std::string_view value) {
    bool has_alphanumeric = false;
    for(const unsigned char c : value) {
        const bool alphanumeric = (c >= 'a' && c <= 'z')
                               || (c >= 'A' && c <= 'Z')
                               || (c >= '0' && c <= '9');
        if(alphanumeric) {
            has_alphanumeric = true;
            continue;
        }
        if(c != ' ' && c != '-' && c != '_')
            throw InvalidArgumentException("Invalid character in frame tag name: ", value);
    }
    if(not has_alphanumeric)
        throw InvalidArgumentException("Frame tag names need at least one letter or number.");
}

std::string FrameTag::toStr() const {
    if(std::holds_alternative<SimpleTag>(name))
        return std::get<SimpleTag>(name);
    if(has_identity())
        return Meta::toStr(std::get<IdentifiedTag>(name));
    return Meta::toStr(std::get<SpatialTag>(name));
}

glz::json_t FrameTag::to_json() const {
    if(std::holds_alternative<SimpleTag>(name))
        return cvt2json(std::get<SimpleTag>(name));
    if(has_identity())
        return cvt2json(std::get<IdentifiedTag>(name));
    return cvt2json(std::get<SpatialTag>(name));
}

FrameTag::operator std::string_view() const {
    if(std::holds_alternative<SimpleTag>(name))
        return std::string_view(std::get<SimpleTag>(name));
    if(has_identity())
        return std::string_view(std::get<IdentifiedTag>(name).name);
    return std::string_view(std::get<SpatialTag>(name).name);
}

bool FrameTag::has_location() const {
    return std::holds_alternative<SpatialTag>(name)
            || std::holds_alternative<IdentifiedTag>(name);
}

std::string_view FrameTag::get_name() const {
    return (std::string_view)*this;
}

Bounds FrameTag::get_location() const {
    if(not has_location())
        throw RuntimeError("FrameTag ", *this, " has no location data.");
    if(std::holds_alternative<IdentifiedTag>(name))
        return std::get<IdentifiedTag>(name).bds;
    return std::get<SpatialTag>(name).bds;
}

bool FrameTag::has_identity() const {
    return std::holds_alternative<IdentifiedTag>(name);
}

uint32_t FrameTag::get_identity() const {
    return std::get<IdentifiedTag>(name).fdx;
}

}
