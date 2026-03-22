#pragma once

#include <commons.pc.h>

namespace pv {

class Blob;
struct CompressedBlob;
using BlobPtr = std::unique_ptr<pv::Blob>;

enum class FilterReason {
    DontTrackTags,
    OutsideRange,
    InsideIgnore,
    OutsideInclude,
    SecondThreshold,
    Category,
    Label,
    TrackConfidenceThreshold,
    SplitFailed,
    OnlySegmentations,
    BdxIgnored,
    Unknown
};

const char* filter_reason_to_str(FilterReason reason);

struct bid {
    static constexpr uint32_t invalid = std::numeric_limits<uint32_t>::max();
    uint32_t _id = invalid;
    bid() = default;
    bid(const bid&) = default;
    bid& operator=(const bid&) = default;
    bid& operator=(bid&&) = default;

    constexpr bid(cmn::unsigned_number auto v) : _id(v) {}

    explicit constexpr operator uint32_t() const {
        return _id;
    }
    explicit constexpr operator int64_t() const { return static_cast<int64_t>(_id); }
    explicit constexpr operator uint64_t() const { return static_cast<uint64_t>(_id); }
    constexpr bool operator==(const bid& other) const {
        return other._id == _id;
    }
    constexpr bool operator!=(const bid& other) const {
        return other._id != _id;
    }
    constexpr bool valid() const { return _id != invalid; }

    constexpr auto operator<=>(const bid& other) const {
        if (!valid() || !other.valid()) {
            throw std::invalid_argument("Comparing to an invalid pv::bid does not produce the desired outcome.");
        }
        return _id <=> other._id;
    }

    std::string toStr() const;
    glz::json_t to_json() const;
    static std::string class_name() { return "blob"; }
    static bid fromStr(cmn::StringLike auto&& str) {
        if (str == "null") {
            return pv::bid();
        }
        return bid(cmn::Meta::fromStr<uint32_t>(str));
    }

    static uint32_t xy2d(uint16_t x, uint16_t y) {
        uint32_t hilbert = 0;
        for (int i = 15; i >= 0; i--) {
            const uint32_t xi = (x & (1 << i)) ? 1 : 0;
            const uint32_t yi = (y & (1 << i)) ? 1 : 0;
            const uint32_t quadrant = (xi ^ yi) | ((yi ^ 1) << 1);
            hilbert = (hilbert << 2) | quadrant;
            if (yi) {
                hilbert ^= 3;
            }
            if (xi) {
                hilbert ^= 2;
            }
        }
        return hilbert;
    }

    static constexpr uint32_t from_data(ushort x0, ushort x1, ushort y0, uint8_t N) {
        assert((uint32_t)x0 < (uint32_t)8192u);
        assert((uint32_t)x1 < (uint32_t)8192u);
        assert((uint32_t)y0 < (uint32_t)8192u);
        return ((uint32_t)((x0 + x1 + 1) / 2) << 19)
                | ((uint32_t(y0) & 0x1FFF) << 6)
                |  (uint32_t(N) % 64);
    }

    static void d2xy(uint32_t d, uint16_t& x, uint16_t& y) {
        x = 0;
        y = 0;
        const int n = 16;
        uint32_t rx = 0;
        uint32_t ry = 0;
        for (int i = 0; i < n; i++) {
            rx = (d >> (2 * i)) & 1;
            ry = (d >> (2 * i + 1)) & 1;
            x |= (rx ^ ry) << i;
            y |= ((rx ^ 1) & ry) << i;
        }
    }

    [[nodiscard]] constexpr cmn::Vec2 calc_position() const {
        const auto x = (_id >> 19) & 0x1FFF;
        const auto y = (_id >> 6) & 0x1FFF;
        return cmn::Vec2(x, y);
    }
};

static_assert(static_cast<uint32_t>(int32_t(-1)) == bid::invalid, "Must be equal to ensure backwards compatibility.");

}

template <>
struct glz::meta<pv::bid> {
    using mimic = std::optional<uint32_t>;
    static constexpr auto read = [](pv::bid& output, const std::optional<uint32_t>& v) {
        output = v.has_value() ? pv::bid{v.value()} : pv::bid{};
    };
    static constexpr auto write = [](const pv::bid& input) -> std::optional<uint32_t> {
        return input.valid() ? std::optional<uint32_t>((uint32_t)input) : std::nullopt;
    };
    static constexpr auto value = glz::custom<read, write>;
};

namespace std
{
    template <>
    struct hash<pv::bid>
    {
        size_t operator()(const pv::bid& k) const
        {
            return std::hash<uint32_t>{}((uint32_t)k);
        }
    };
}
