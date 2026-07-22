#pragma once

#include <commons.pc.h>

namespace cmn {

/// aggregate data struct that contains only a text-based ID,
/// optionally localized via a bounding box
struct FrameTag {
    std::variant<std::pair<Bounds, std::string>, std::string> name;
    std::string toStr() const;
    glz::json_t to_json() const;
    static FrameTag fromStr(StringLike auto && str) {
        const auto value = utils::string_like_view(
            std::forward<decltype(str)>(str));
        if(utils::beginsWith(value, '[')
           && utils::endsWith(value, ']'))
        {
            auto localized = Meta::fromStr<std::pair<Bounds,std::string>>(value);
            validate_name(localized.second);
            return FrameTag{.name = std::move(localized)};
        }

        validate_name(value);
        return FrameTag{.name = std::string(value)};
    }
    bool operator==(const FrameTag& other) const = default;
    consteval static std::string_view class_name() { return "Tag"; }

    bool has_location() const;
    Bounds get_location() const;
    std::string_view get_name() const;

    explicit operator std::string_view() const;

    auto operator<=>(const FrameTag& other) const {
        return name <=> other.name;
    }

private:
    static void validate_name(std::string_view);
};

}
