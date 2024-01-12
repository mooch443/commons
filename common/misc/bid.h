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
    std::string toStr() const;
    static std::string class_name() { return "BlobWeakPtr"; }
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
    nlohmann::json to_json() const;
    static std::string class_name() { return "blob"; }
    static bid fromStr(const std::string& str);
    
    static uint32_t xy2d(uint16_t x, uint16_t y) {
        uint32_t hilbert = 0;
        for (int i = 15; i >= 0; i--) {
            uint32_t xi = (x & (1 << i)) ? 1 : 0;
            uint32_t yi = (y & (1 << i)) ? 1 : 0;

            // Interleave the bits in xi and yi to form the next 2 bits of the Hilbert value.
            uint32_t quadrant = (xi ^ yi) | ((yi ^ 1) << 1);

            // Rotate the current bits of the Hilbert value to the left by 2, and then add the new quadrant value.
            hilbert = (hilbert << 2) | quadrant;

            // If the next bit in y is set, flip the current quadrant.
            if (yi) {
                hilbert ^= 3; // 3 is 11 in binary
            }

            // If the next bit in x is set, flip the second bit of the current quadrant.
            if (xi) {
                hilbert ^= 2; // 2 is 10 in binary
            }
        }
        return hilbert;
    }

    /**
     * @brief Encodes the given x0, x1, y0, and N values into a unique bid identifier using custom bit allocation.
     *
     * The old-style encoding uses 12 bits for the average X-coordinate (x0 + x1) / 2, 12 bits for the Y-coordinate (y0), and 8 bits for N.
     * The new-style encoding increases the resolution of the X and Y coordinates by using 13 bits each, and reduces the resolution of N to 6 bits using a modulus operation.
     * This results in a total of 32 bits for the uint32_t encoding.
     *
     * | 32    31    30    29    28    27    26    25    24 | 23    22    21    20    19    18    17    16 | 15    14    13    12    11    10    09    08 |
     * |       Avg X-coordinate (13 bits)     |       Y-coordinate (13 bits)       |   N (6 bits)  |
     *
     * Example combinations for the new-style custom encoding:
     * 1. (x0=4000, x1=4500, y0=2000, N=100)
     *    Average X-coordinate: (4000 + 4500) / 2 = 4250
     *    N (mod 64): 100 mod 64 = 36
     *    Encoded: 1000010101010 11111010000 100100
     * 2. (x0=1000, x1=1050, y0=1500, N=200)
     *    Average X-coordinate: (1000 + 1050) / 2 = 1025
     *    N (mod 64): 200 mod 64 = 8
     *    Encoded: 0001111111001 01011101100 001000
     * 3. (x0=2047, x1=2047, y0=2047, N=255)
     *    Average X-coordinate: (2047 + 2047) / 2 = 2047
     *    N (mod 64): 255 mod 64 = 63
     *    Encoded: 0111111111111 01111111111 111111
     *
     * @param x0 - The lower x-coordinate of the object.
     * @param x1 - The upper x-coordinate of the object.
     * @param y0 - The y-coordinate of the object.
     * @param N - The length (number of lines) of the object.
     *
     * @return The encoded unique bid identifier using custom bit allocation.
     */
    static constexpr uint32_t from_data(ushort x0, ushort x1, ushort y0, uint8_t N) {
        assert((uint32_t)x0 < (uint32_t)8192u);
        assert((uint32_t)x1 < (uint32_t)8192u);
        assert((uint32_t)y0 < (uint32_t)8192u);
        //return xy2d((uint32_t)((x0 + x1 + 1) / 2), y0);
        return ((uint32_t)((x0 + x1 + 1) / 2) << 19)
                | ((uint32_t(y0) & 0x1FFF) << 6)
                |  (uint32_t(N) % 64);
    }

    static void d2xy(uint32_t d, uint16_t &x, uint16_t &y) {
        x = 0;
        y = 0;
        int n = 16;
        uint32_t rx, ry;
        for (int i = 0; i < n; i++) {
            rx = (d >> (2*i)) & 1;
            ry = (d >> (2*i + 1)) & 1;
            x |= (rx ^ ry) << i;
            y |= ((rx ^ 1) & ry) << i;
        }
    }
    
    /**
     * @brief Calculates the position (x, y) from the encoded bid identifier.
     *
     * The decoding uses 13 bits for the X-coordinate, 13 bits for the Y-coordinate, ignoring the 6 bits for N.
     *
     * @return The x and y position as a cmn::Vec2.
     */
    [[nodiscard]] constexpr cmn::Vec2 calc_position() const {
        //uint16_t x, y;
        //d2xy(_id, x, y);
        
        auto x = (_id >> 19) & 0x1FFF;
        auto y = (_id >> 6) & 0x1FFF;
        //auto N = id & 0x3F;
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
