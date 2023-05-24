#pragma once

#include <commons.pc.h>
#include <misc/vec2.h>

namespace pv {

class Blob;
struct CompressedBlob;
using BlobPtr = std::unique_ptr<pv::Blob>;

/**
 * Enum representing the different reasons a blob may be filtered out.
 */
enum class FilterReason {
    DontTrackTags,
    OutsideRange,
    InsideIgnore,
    OutsideInclude,
    SecondThreshold,
    Category,
    Label,
    LabelConfidenceThreshold,
    SplitFailed,
    OnlySegmentations,
    
    Unknown
};

/**
 * A weak pointer to a Blob object.
 */
struct BlobWeakPtr {
    pv::Blob* ptr;
    
    constexpr BlobWeakPtr() = default;
    constexpr BlobWeakPtr(pv::Blob* p) : ptr(p) {}
    constexpr operator pv::Blob*() const {
        return ptr;
    }
    constexpr pv::Blob& operator*() {return *ptr;}
    constexpr const pv::Blob& operator*() const {return *ptr;}
    constexpr pv::Blob* operator->() {return ptr;}
    constexpr const pv::Blob* operator->() const {return ptr;}
};

/**
 * Represents the ID of an object (blob) that is an object of interest in a given video frame.
 */
struct bid {
    static constexpr uint32_t invalid = std::numeric_limits<uint32_t>::max();
    uint32_t _id = invalid;
    bid() = default;
    bid(const bid&) = default;
    bid& operator=(const bid&) = default;
    bid& operator=(bid&&) = default;
    
    constexpr bid(cmn::unsigned_number auto v) : _id(v) {}
    //constexpr bid(cmn::signed_number auto v) : _id(v) {}
    
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
    //constexpr bid(uint32_t b) : _id(b) {}
    constexpr bool valid() const { return _id != invalid; }
    
    constexpr auto operator<=>(const bid& other) const {
//#ifndef NDEBUG
        if(!valid() || !other.valid())
            throw std::invalid_argument("Comparing to an invalid pv::bid does not produce the desired outcome.");
//#endif
        return _id <=> other._id;
    }
    
    std::string toStr() const;
    static std::string class_name() { return "blob"; }
    static bid fromStr(const std::string& str);

    /**
     * @brief Encodes the given x0, x1, y0, and N values into a unique bid identifier using custom bit allocation.
     *
     * The old-style encoding uses 12 bits for the average X-coordinate (x0 + x1) / 2, 12 bits for the Y-coordinate (y0), and 8 bits for N.
     * This results in a total of 32 bits for the uint32_t encoding.
     *
     * | 32    31    30    29    28    27    26    25    24 | 23    22    21    20    19    18    17    16 | 15    14    13    12    11    10    09    08 |
     * |       Avg X-coordinate (12 bits)     |       Y-coordinate (12 bits)        |   N (8 bits)  |
     *
     * Example combinations for the custom encoding:
     * 1. (x0=500, x1=550, y0=600, N=100)
     *    Average X-coordinate: (500 + 550) / 2 = 525
     *    Encoded: 010000110101 1001011000 01100100
     * 2. (x0=1000, x1=1050, y0=1500, N=50)
     *    Average X-coordinate: (1000 + 1050) / 2 = 1025
     *    Encoded: 111110100001 1011101100 00110010
     * 3. (x0=2047, x1=2047, y0=2047, N=255)
     *    Average X-coordinate: (2047 + 2047) / 2 = 2047
     *    Encoded: 111111111111 1111111111 11111111
     *
     * @param x0 - The lower x-coordinate of the object.
     * @param x1 - The upper x-coordinate of the object.
     * @param y0 - The y-coordinate of the object.
     * @param N - The length (number of lines) of the object.
     *
     * @return The encoded unique bid identifier using custom bit allocation.
     */
    static constexpr uint32_t from_data(ushort x0, ushort x1, ushort y0, uint8_t N) {
        assert((uint32_t)x0 < (uint32_t)4096u);
        assert((uint32_t)x1 < (uint32_t)4096u);
        assert((uint32_t)y0 < (uint32_t)4096u);
        
        return (uint32_t(x0 + (x1 - x0) / 2) << 20)
                | ((uint32_t(y0) & 0x00000FFF) << 8)
                |  (uint32_t(N)  & 0x000000FF);
    }
    
    /**
     * @brief Calculates the position (x, y) from the encoded bid identifier.
     *
     * The decoding uses 12 bits for the X-coordinate, 12 bits for the Y-coordinate, ignoring the 8 bits for N.
     *
     * @return The x and y position as a cmn::Vec2.
     */
    [[nodiscard]] constexpr cmn::Vec2 calc_position() const {
        auto x = (_id >> 20) & 0x00000FFF;
        auto y = (_id >> 8) & 0x00000FFF;
        //auto N = id & 0x000000FF;
        return cmn::Vec2(x, y);
    }
    //static uint32_t id_from_position(const cmn::Vec2&);
    static bid from_blob(const pv::Blob& blob);
    static bid from_blob(const pv::CompressedBlob& blob);
};


static_assert(int32_t(-1) == (uint32_t)bid::invalid, "Must be equal to ensure backwards compatibility.");

}

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
