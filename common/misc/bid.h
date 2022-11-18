#pragma once

#include <commons.pc.h>
#include <misc/vec2.h>

namespace pv {

class Blob;
struct CompressedBlob;
using BlobPtr = std::shared_ptr<pv::Blob>;

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

    static constexpr uint32_t from_data(ushort x0, ushort x1, ushort y0, uint8_t N) {
        assert((uint32_t)x0 < (uint32_t)4096u);
        assert((uint32_t)x1 < (uint32_t)4096u);
        assert((uint32_t)y0 < (uint32_t)4096u);
        
        return (uint32_t(x0 + (x1 - x0) / 2) << 20)
                | ((uint32_t(y0) & 0x00000FFF) << 8)
                |  (uint32_t(N)  & 0x000000FF);
    }
    
    constexpr cmn::Vec2 calc_position() const {
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
