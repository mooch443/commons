#pragma once

#include <commons.pc.h>

namespace cmn::CPULabeling {

template<typename Storage = uint32_t, uint8_t BigWord = 13, uint8_t SmallWord = uint8_t(CHAR_BIT * sizeof(Storage)) - BigWord * 2u>
struct HLine {
    /**
     * 13bit x0 | 6 bit x1 | 13 bit y
     */
    Storage x0x1y{0};
    
    static constexpr uint8_t offset_x0 = SmallWord;
    static constexpr uint8_t offset_x1 = 0;
    static constexpr uint8_t offset_y = SmallWord + BigWord;
    static constexpr Storage bit_size_x0 = (1u << BigWord) - 1u;
    static constexpr Storage bit_size_x1 = (1u << SmallWord) - 1u;
    static constexpr Storage bit_size_y = (1u << BigWord) - 1u;
    
    static constexpr Storage mask_x0 = bit_size_x0 << offset_x0;// 0xFFF80000;
    static constexpr Storage mask_x1 = bit_size_x1 << offset_x1;//0x0007E000;
    static constexpr Storage mask_y  = bit_size_y  << offset_y;//0x00001FFF;
    
    auto xy() const noexcept { return x0x1y; }
    
    coord_t x0() const noexcept {
        return coord_t((x0x1y & mask_x0) >> offset_x0);
    }
    constexpr coord_t x1() const noexcept {
        // Width is stored in mask_x1 bits; add to x0 from mask_x0 bits
        return coord_t(((x0x1y & mask_x0) >> offset_x0)
                     + ((x0x1y & mask_x1) >> offset_x1));
    }
    coord_t y() const noexcept {
        return coord_t((x0x1y & mask_y) >> offset_y);
    }
    
    void x0(coord_t x)
#ifdef NDEBUG
    noexcept
#endif
    {
#ifndef NDEBUG
        if(uint32_t(x) > bit_size_x0)
            throw std::invalid_argument("x0 is too big.");
        if(x1() < x)
            throw U_EXCEPTION("new x0=",x," is smaller than x1=",x1()," value=", xy());
#endif
        x0x1y = (x0x1y & mask_y)
                | ((Storage(x) << offset_x0) & mask_x0)
                | ((Storage(x1() - x) << offset_x1) & mask_x1);
        
        assert(x0() == x);
    }
    void x1(coord_t x)
#ifdef NDEBUG
    noexcept
#endif
    {
#ifndef NDEBUG
        if(uint32_t(x0()) > uint32_t(x))
            throw U_EXCEPTION("x=",x," is smaller than x0=",x0(), " value=",xy());
        if(uint32_t(x) - uint32_t(x0()) > bit_size_x1)
            throw std::invalid_argument("x1 is too big");
#endif
        
        x0x1y = (x0x1y & (mask_x0 | mask_y))
                | ((Storage(x - x0()) << offset_x1) & mask_x1);
        
        assert(x1() == x);
    }
    void y(coord_t y)
#ifdef NDEBUG
    noexcept
#endif
    {
        x0x1y = (x0x1y & (mask_x0 | mask_x1))
                | ((Storage(y) << offset_y) & mask_y);
        assert(y == this->y());
    }
    
constexpr bool operator<(const HLine& other) const noexcept {
    //! Compares two HorizontalLines and sorts them by y and x0 coordinates
    //  (top-bottom, left-right) by combining fields into a single integer comparison
    constexpr Storage cmp_mask = mask_y | mask_x0;
    return (x0x1y & cmp_mask) < (other.x0x1y & cmp_mask);
}
    
    constexpr bool overlap_x(const HLine& other) const noexcept {
        return other.x1() >= x0()-1 && other.x0() <= x1()+1;
    }
    
    constexpr operator HorizontalLine() const noexcept {
        return HorizontalLine(y(), x0(), x1());
    }
    
    HLine() = default;
    HLine(const HorizontalLine& line)
#ifdef NDEBUG
        noexcept
#endif
    : x0x1y(       ((Storage(line.x0) << offset_x0) & mask_x0)
                | (((Storage(line.x1) - Storage(line.x0)) << offset_x1) & mask_x1)
                |  ((Storage(line.y) << offset_y) & mask_y)
            )
    {
#ifndef NDEBUG
        if(Storage(line.x1) - Storage(line.x0) > bit_size_x1)
            throw std::invalid_argument("x1 is too big");
        if(Storage(line.x0) > bit_size_x0)
            throw std::invalid_argument("x0 is too big.");
        if(Storage(line.y) > bit_size_y)
            throw std::invalid_argument("y is too big.");
#endif

        assert(x0() == line.x0);
        assert(x1() == line.x1);
        assert(y() == line.y);
    }
    
    HLine(coord_t x0, coord_t x1, coord_t y)
    : x0x1y(   ((Storage(x0) << offset_x0) & mask_x0)
            | (((Storage(x1) - Storage(x0)) << offset_x1) & mask_x1)
            |  ((Storage(y) << offset_y) & mask_y)
            )
    {
#ifndef NDEBUG
        if(Storage(x1) - Storage(x0) > bit_size_x1)
            throw std::invalid_argument("x1 is too big: "+ std::to_string(x0) + "-"+std::to_string(x1));
        if(Storage(x0) > bit_size_x0)
            throw std::invalid_argument("x0 is too big.");
        if(Storage(y) > bit_size_y)
            throw std::invalid_argument("y is too big.");
#endif
        
        assert(this->x0() == x0);
        assert(this->x1() == x1);
        assert(this->y() == y);
    }
    
    static std::string class_name() {
        return "HLine";
    }
    
    std::string toStr() const {
        return "HLine<" + Meta::toStr(y()) + ";" + Meta::toStr(x0()) + "," + Meta::toStr(x1()) + ">";
    }
};

using Pixel = const uchar*;
using Line32 = HLine<uint32_t>;
using Line64 = HLine<uint64_t, 24>;

using Line_t = Line64;

}
