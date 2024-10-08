#ifndef _BASIC_TYPE_H
#define _BASIC_TYPE_H

#include <commons.pc.h>
#include <misc/colors.h>

namespace cmn::gui {
    inline std::ostream & operator <<(std::ostream &os, const Bounds& s) {
        //assert(uint(x) <= USHRT_MAX && uint(y) <= USHRT_MAX);
        uint64_t all = (uint16_t(s.y) & 0xFFFF);
        all    |= ((uint64_t(s.x) << 16) & 0xFFFF0000);
        all    |= ((uint64_t(s.height) << 32) & 0xFFFF00000000);
        all    |= ((uint64_t(s.width) << 48) & 0xFFFF000000000000);
        
        return os << all;
    }

    class Vertex {
        GETTER_NCONST(ImU32, color);
        GETTER_NCONST(Vec2, position);
        
    public:
        Vertex(Float2_t x, Float2_t y, const Color& color = Color())
            : _color((ImU32)color), _position(x, y)
        {}
        
        template<typename T>
        Vertex(const T& position, const Color& color, typename std::enable_if<std::is_same<decltype(position.x),decltype(position.y)>::value, bool>::type * = NULL)
            : Vertex(position.x, position.y, color)
        {}
        
        Vertex(const Vec2& position = Vec2(), const Color& color = Color())
            : Vertex(position.x, position.y, color)
        {}
        
        operator cv::Point2f() const {
            return _position;
        }
        
        operator Vec2() const {
            return _position;
        }
        
        bool operator==(const Vertex& other) const {
            return int(other._position.x) == int(_position.x) && int(other._position.y) == int(_position.y) && other._color == _color;
        }
        
        bool operator!=(const Vertex& other) const {
            return ! operator==(other);
        }
        
        Color clr() const {
            return Color(_color);
        }
        
        std::ostream &operator <<(std::ostream &os);
        std::string toStr() const;
        static std::string class_name() { return "Vertex"; }
    };
    
    enum Style
    {
        Regular       = 0,      //! Regular characters, no style
        Bold          = 1 << 0, //! Bold characters
        Italic        = 1 << 1, //! Italic characters
        Underlined    = 1 << 2, //! Underlined characters
        StrikeThrough = 1 << 3, //! Strike through characters
        Monospace     = 1 << 4, //! Code style
        Symbols       = 1 << 5  //! Symbols
    };
    
    enum Align {
        Left = 0,
        Center = 1,
        Right = 2,
        VerticalCenter = 3
    };
    
    struct Font {
        Float2_t size;
        uint32_t style;
        Align align;
        
        constexpr Font()
            : size(1), style(Style::Regular), align(Align::Left)
        { }
        
        template <typename T = Style, typename K = Align>
        constexpr Font(Float2_t s, T style, K align = Align::Left, typename std::enable_if<std::is_same<T, Style>::value && std::is_same<K, Align>::value, bool>::type * = NULL)
            : size(s), style(style), align(align)
        { }
        
        template <typename K = Align>
        constexpr Font(Float2_t s, K align = Align::Left, typename std::enable_if<std::is_same<K, Align>::value, bool>::type* = NULL)
            : Font(s, Style::Regular, align)
        {}
        
        bool operator==(const Font& other) const {
            return other.size == size && other.style == style && other.align == align;
        }
        bool operator!=(const Font& other) const {
            return !operator==(other);
        }
        
        std::string toStr() const;
        static std::string class_name() { return "Font"; }
    };
}

#endif
