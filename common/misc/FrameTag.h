#pragma once

#include <commons.pc.h>

namespace cmn {

struct IdentifiedTag {
    uint32_t fdx;
    Bounds bds;
    std::string name;
    
    auto operator<=>(const IdentifiedTag&) const = default;
    glz::json_t to_json() const;
    std::string toStr() const;
    static IdentifiedTag fromStr(StringLike auto && str) {
        auto tmp = Meta::fromStr<std::tuple<uint32_t, Bounds, std::string>>(std::forward<decltype(str)>(str));
        return {
            .fdx = std::get<0>(tmp),
            .bds = std::get<1>(tmp),
            .name = std::get<2>(tmp)
        };
    }
    
    static consteval std::string_view class_name() { return "IdentifiedTag"; }
};

struct SpatialTag {
    Bounds bds;
    std::string name;
    
    auto operator<=>(const SpatialTag&) const = default;
    glz::json_t to_json() const;
    std::string toStr() const;
    static SpatialTag fromStr(StringLike auto && str) {
        auto tmp = Meta::fromStr<std::tuple<Bounds, std::string>>(std::forward<decltype(str)>(str));
        return {
            .bds = std::get<0>(tmp),
            .name = std::get<1>(tmp)
        };
    }
    
    static consteval std::string_view class_name() { return "SpatialTag"; }
};

using SimpleTag = std::string;

/// aggregate data struct that contains only a text-based ID,
/// optionally localized via a bounding box
struct FrameTag {
    std::variant<IdentifiedTag, SpatialTag, SimpleTag> name;
    std::string toStr() const;
    glz::json_t to_json() const;
    static FrameTag fromStr(StringLike auto && str) {
        const auto value = utils::string_like_view(
            std::forward<decltype(str)>(str));
        if(utils::beginsWith(value, '[')
           && utils::endsWith(value, ']'))
        {
            auto parts = util::parse_array_parts(util::truncate(value));
            if(parts.size() == 3) {
                auto identified = Meta::fromStr<IdentifiedTag>(value);
                validate_name(identified.name);
                return FrameTag{.name = std::move(identified)};
            }
            auto localized = Meta::fromStr<SpatialTag>(value);
            validate_name(localized.name);
            return FrameTag{.name = std::move(localized)};
        }

        validate_name(value);
        return FrameTag{.name = SimpleTag(value)};
    }
    bool operator==(const FrameTag& other) const = default;
    consteval static std::string_view class_name() { return "Tag"; }

    bool has_location() const;
    Bounds get_location() const;
    std::string_view get_name() const;
    bool has_identity() const;
    uint32_t get_identity() const;

    explicit operator std::string_view() const;
    std::vector<int64_t> npz_representation_1d(const std::set<std::string_view>& unique) const;

    auto operator<=>(const FrameTag& other) const {
        return name <=> other.name;
    }

private:
    static void validate_name(std::string_view);
};

}
