#pragma once

#include <commons.pc.h>

namespace cmn {
    using Float2_t = float;

    /// a constructor literal for the float2_t type
    constexpr inline Float2_t operator ""_F(long double value) {
        return static_cast<Float2_t>(value);
    }

    /// the same but for integer
    constexpr inline Float2_t operator ""_F(unsigned long long value) {
        return static_cast<Float2_t>(value);
    }

#if CMN_WITH_IMGUI_INSTALLED
    #define IMGUI_CONSTRUCTOR \
        Vector2D(const ImVec2& v) noexcept : Vector2D(v.x, v.y) {} \
        operator ImVec2() const { return ImVec2(A(), B()); }
#else
    #define IMGUI_CONSTRUCTOR
#endif

#define Vector2D_COMMON_FUNCTIONS \
    static consteval self_type zeros() { return self_type{Scalar(0), Scalar(0)}; } \
    template<typename V> \
    Vector2D(const cv::Point_<V>& v) noexcept : Vector2D(v.x, v.y) {} \
    template<typename V> \
    Vector2D(const cv::Size_<V>& v) noexcept : Vector2D(v.width, v.height) {} \
    template<typename V> operator cv::Point_<V> () const { return cv::Point_<V>(A(), B()); } \
    template<typename V> operator cv::Size_<V> () const { return cv::Size_<V>(A(), B()); } \
    \
    IMGUI_CONSTRUCTOR \
    \
    constexpr Scalar min() const { return std::min(A(), B()); } \
    constexpr Scalar max() const { return std::max(A(), B()); } \
    constexpr Scalar mean() const { return 0.5_F * (A() + B()); } \
    constexpr Vector2D abs() const { return self_type(std::abs(A()), std::abs(B())); } \
    constexpr bool empty() const { return operator==(zeros()); } \
    \
    template<bool K> \
    constexpr Vector2D mul(const Vector2D<Scalar, K>& other) const { \
        return Vector2D{ A() * other.A(), B() * other.B() }; \
    } \
    template<bool K> \
    constexpr Vector2D div(const Vector2D<Scalar, K>& other) const { \
        return Vector2D{ A() / other.A(), B() / other.B() }; \
    } \
    constexpr Vector2D mul(Scalar a, Scalar b) const { \
        return Vector2D{ A() * a, B() * b }; \
    } \
    constexpr Vector2D div(Scalar a, Scalar b) const { \
        return Vector2D{ A() / a, B() / b }; \
    } \
    \
    constexpr Vector2D mul(const Scalar& other) const { \
        return Vector2D(A() * other, B() * other); \
    } \
    constexpr Vector2D div(const Scalar& other) const { \
        return Vector2D(A() / other, B() / other); \
    } \
    \
    constexpr Vector2D T() const { \
        return Vector2D(B(), A()); \
    } \
    \
    template<Scalar (*F)(Scalar)> \
    constexpr Vector2D map() const { \
        return Vector2D(F(A()), F(B())); \
    } \
    \
    constexpr Vector2D map(std::function<Scalar(Scalar)>&& fn) const { \
        return Vector2D(fn(A()), fn(B())); \
    } \
    \
    static constexpr inline bool almost_equal(Scalar a, Scalar b) { \
        constexpr auto epsilon = Scalar(0.001); \
        return std::abs(a - b) <= epsilon; \
    } \
    \
    template<typename T, bool K> \
    constexpr bool Equals(const Vector2D<T, K>& other) const { \
        return almost_equal(A(), other.A()) && almost_equal(B(), other.B()); \
    } \
    \
    constexpr auto operator<=>(const Vector2D& bla) const = default; \
    \
    template<typename V, bool K> \
    constexpr bool operator==(const Vector2D<V, K>& other) const { \
        return other.A() == A() && other.B() == B(); \
    } \
    template<typename V, bool K> \
    constexpr bool operator!=(const Vector2D<V, K>& other) const { \
        return other.A() != A() || other.B() != B(); \
    } \
    \
    template<typename V, bool K> \
    constexpr bool operator<(const Vector2D<V, K>& other) const { \
        return other.B() < B() || (other.B() == B() && other.A() < A()); \
    } \
    \
    template<typename V, bool K> \
    constexpr bool operator>(const Vector2D<V, K>& other) const { \
        return other.B() > B() || (other.B() == B() && other.A() > A()); \
    } \
    \
    template<typename V, bool K> \
    constexpr Vector2D& operator+=(const Vector2D<V, K>& other) { \
        A() += other.A(); \
        B() += other.B(); \
        return *this; \
    } \
    \
    template<typename V, bool K> \
    constexpr Vector2D& operator-=(const Vector2D<V, K>& other) { \
        A() -= other.A(); \
        B() -= other.B(); \
        return *this; \
    } \
    \
    template<typename K> \
    constexpr Vector2D& operator+=(const cv::Point_<K>& other) { \
        A() += other.x; \
        B() += other.y; \
        return *this; \
    } \
    \
    template<typename K> \
    constexpr Vector2D& operator-=(const cv::Point_<K>& other) { \
        A() -= other.x; \
        B() -= other.y; \
        return *this; \
    } \
    \
    template<typename V, bool K> \
    constexpr V dot(const Vector2D<V, K>& other) const { \
        return A()*other.A() + B()*other.B(); \
    } \
    \
    constexpr Vector2D perp() const { \
        return Vector2D(B(), -A()); \
    } \
    \
    constexpr Scalar length() const { \
        return std::sqrt(sqlength()); \
    } \
    \
    template<typename K> \
    static constexpr K square(K k) { \
        return k * k; \
    } \
    \
    constexpr Scalar sqlength() const { \
        return square(A()) + square(B()); \
    } \
    \
    constexpr Vector2D normalize() const { \
        auto L = length(); \
        return Scalar(L != 0) * (*this / (Scalar(L == 0) + L)); \
    } \
    \
    constexpr Vector2D clip(Scalar start, Scalar end) const { \
        return Vector2D(saturate(A(), start, end), saturate(B(), start, end)); \
    } \
    constexpr Vector2D clip(Scalar start) const { \
        return Vector2D(A() < start ? start : A(), B() < start ? start : B()); \
    } \
    \
    template<bool K> \
    constexpr Vector2D clip(const Vector2D<Scalar, K>& start, const Vector2D<Scalar, K>& end) const { \
        return Vector2D(saturate(A(), start.A(), end.A()), saturate(B(), start.B(), end.B())); \
    } \
    \
    /* Element-wise reciprocal (1/x) */ \
    constexpr Vector2D reciprocal() const { \
        return Vector2D(Scalar(1) / A(), Scalar(1) / B()); \
    } \
    \
    constexpr Scalar atan2() const { \
        return std::atan2(B(), A()); \
    } \
    \
    constexpr Vector2D& operator+=(Scalar other) { A() += other; B() += other; return *this; } \
    constexpr Vector2D& operator-=(Scalar other) { A() -= other; B() -= other; return *this; } \
    constexpr Vector2D& operator*=(Scalar other) { A() *= other; B() *= other; return *this; } \
    constexpr Vector2D& operator/=(Scalar other) { A() /= other; B() /= other; return *this; } \
    \
    template<typename S> requires is_numeric<S> \
    constexpr Vector2D operator/(S other) const { return Vector2D{ A() / Scalar(other), B() / Scalar(other) }; } \
    template<typename S> requires is_numeric<S> \
    constexpr Vector2D operator*(S other) const { return Vector2D{ A() * Scalar(other), B() * Scalar(other) }; } \
    template<typename S> requires is_numeric<S> \
    constexpr Vector2D operator-(S other) const { return Vector2D{ A() - Scalar(other), B() - Scalar(other) }; } \
    template<typename S> requires is_numeric<S>  \
    constexpr Vector2D operator+(S other) const { return Vector2D{ A() + Scalar(other), B() + Scalar(other) }; } \
    \
    constexpr self_type operator+(other_type other) const { \
        return self_type{ A() + other.A(), B() + other.B() }; \
    } \
    constexpr self_type operator+(self_type other) const { \
        return self_type{ A() + other.A(), B() + other.B() }; \
    } \
    \
    constexpr self_type operator-(other_type other) const { \
        return self_type{ A() - other.A(), B() - other.B() }; \
    } \
    constexpr self_type operator-(self_type other) const { \
        return self_type{ A() - other.A(), B() - other.B() }; \
    } \
    \
    constexpr self_type operator -() const { return { -A(), -B() }; } \
    \
    constexpr self_type operator +() const { return *this; } \
    \
    std::string toStr() const { \
        return "[" + Meta::toStr(A()) + "," + Meta::toStr(B()) + "]"; \
    } \
    glz::json_t to_json() const { \
        return { A(), B() }; \
    } \
    \
    static Vector2D fromStr(cmn::StringLike auto&& str) \
    { \
        auto vec = Meta::fromStr<std::vector<Scalar>>(str); \
        if(vec.empty()) \
            return Vector2D(); \
        if(vec.size() != 2) \
            throw std::invalid_argument("Can only initialize Vector2D with two or no elements. ('"+(std::string)str+"')"); \
        return Vector2D(vec[0], vec[1]); \
    }

template<typename Scalar, bool flag>
struct Vector2D
{ Vector2D() = delete; };

template<typename Scalar>
struct Vector2D<Scalar, true>
{
    using self_scalar = Scalar;
    using self_type = Vector2D<Scalar, true>;
    using other_type = Vector2D<Scalar, false>;
    Scalar x{0}, y{0};

    Vector2D() = default;
    
    constexpr Vector2D(Scalar a) : x(a), y(a) {}
    constexpr Vector2D(Scalar a, Scalar b) : x(a), y(b) {}
    constexpr Vector2D(const other_type& v) : x(v.width), y(v.height) {}
    
    template<typename OtherScalar, bool isSize>
        requires (not std::same_as<OtherScalar, Scalar>)
    constexpr explicit Vector2D(const Vector2D<OtherScalar, isSize>& other)
        : x(static_cast<Scalar>(other.A())), y(static_cast<Scalar>(other.B()))
    { }

    constexpr const Scalar& A() const { return x; }
    constexpr const Scalar& B() const { return y; }

    constexpr Scalar& A() { return x; }
    constexpr Scalar& B() { return y; }
    
    constexpr Vector2D rotate(Scalar angle) const {
        return Vector2D {
            x * cmn::cos(angle) - y * cmn::sin(angle),
            x * cmn::sin(angle) + y * cmn::cos(angle)
        };
    }
    
    static constexpr Vector2D from_angle(Scalar angle) {
        return {
            cmn::cos(angle),
            cmn::sin(angle)
        };
    }

    Vector2D_COMMON_FUNCTIONS

    static std::string class_name() {
        return "vec";
    }
};

template<typename Scalar>
struct Vector2D<Scalar, false>
{
    using self_type = Vector2D<Scalar, false>;
    using other_type = Vector2D<Scalar, true>;
    Scalar width{0}, height{0};

    Vector2D() = default;
    
    constexpr Vector2D(Scalar a) : width(a), height(a) {}
    constexpr Vector2D(Scalar a, Scalar b) : width(a), height(b) {}
    constexpr Vector2D(const other_type& v) : width(v.x), height(v.y) {}
    explicit Vector2D(const cv::Mat& m) : width(m.cols), height(m.rows) {}

    constexpr const Scalar& A() const { return width; }
    constexpr const Scalar& B() const { return height; }

    constexpr Scalar& A() { return width; }
    constexpr Scalar& B() { return height; }

    Vector2D_COMMON_FUNCTIONS

    static std::string class_name() {
        return "size";
    }
};

typedef Vector2D<Float2_t, true> Vec2;
typedef Vector2D<Float2_t, false> Size2;

extern template struct Vector2D<Float2_t, true>;
extern template struct Vector2D<Float2_t, false>;

//static_assert(std::is_trivial_v<Vec2>, "vec2 trivial");
//static_assert(std::is_trivial_v<Size2>, "size2 trivial");

template<typename T>
concept number_type = std::floating_point<T> || std::integral<T>;

#define RScalarFloat2operator(SIGN) \
template<typename Scalar, bool K, typename S1> \
constexpr Vector2D<Scalar, K> operator SIGN (S1 s, const Vector2D<Scalar, K>& v) { return { v.A() SIGN static_cast<Scalar>(s), v.B() SIGN static_cast<Scalar>(s) }; }
    
RScalarFloat2operator(*)
    
template<typename V, bool K>
constexpr inline Vector2D<V, K> operator+(const Vector2D<V, K>& w, const cv::Point_<V>& v) {
    return { v.x + w.A(), v.y + w.B() };
}
    
template<typename V, bool K>
constexpr inline Vector2D<V, K> operator-(const Vector2D<V, K>& w, const cv::Point_<V>& v) {
    return { v.x - w.A(), v.y - w.B() };
}
    
template<typename V, bool K>
inline std::ostream &operator <<(std::ostream &os, const Vector2D<V, K>& obj) {
    uint _x = (uint)round(obj.A()), _y = (uint)round(obj.B());
    //assert(obj.A() >= SHRT_MIN && obj.B() >= SHRT_MIN && obj.A() <= SHRT_MAX && obj.B() <= SHRT_MAX);
        
    uint both = (_x << 16) & 0xFFFF0000;
    both |= _y & 0xFFFF;
        
    return os << both;
}

#undef RScalarFloat2operator
    
template<typename V, bool K>
inline auto atan2(const Vector2D<V, K>& vector) {
    return ::atan2(vector.y, vector.x);
}

template<typename Scalar, bool K>
inline auto length(const Vector2D<Scalar, K>& v) {
    return v.length();
}

template <typename T0, bool K0, typename T1, bool K1>
auto sqdistance(const Vector2D<T0, K0>& point0, const Vector2D<T1, K1>& point1) -> T1 {
    return SQR(point1.A() - point0.A()) + SQR(point1.B() - point0.B());
}

template <typename Scalar, bool K0, bool K1>
auto euclidean_distance(const Vector2D<Scalar, K0>& point0, const Vector2D<Scalar, K1>& point1) -> Scalar {
    return cmn::sqrt(sqdistance(point0, point1));
}

static_assert(sizeof(Float2_t) * 2 == sizeof(Vec2), "This must be true in order to ensure the union hack below works.");

/**
 * -----------------------------------------------
 *                     BOUNDS
 * -----------------------------------------------
 */

class Bounds {
public:
    Float2_t x=0, y=0, width=0, height=0;
        
public:
    Bounds(Bounds&& other) = default;
        
    constexpr Bounds(const Bounds& other)
        : Bounds(other.x, other.y, other.width, other.height)
    {}
        
    constexpr Bounds(double _x = 0, double _y = 0, double w = 0, double h = 0)
        : x(static_cast<Float2_t>(_x)), y(static_cast<Float2_t>(_y)), width(static_cast<Float2_t>(w)), height(static_cast<Float2_t>(h))
    {}
        
    explicit constexpr Bounds(const Vec2& pos,
                        const Size2& dim = Size2())
        : Bounds(pos.x, pos.y, dim.width, dim.height)
    {}
        
    explicit constexpr Bounds(const Size2& dim)
        : Bounds(0, 0, dim.width, dim.height)
    {}
        
#if CMN_WITH_IMGUI_INSTALLED
    Bounds(const ImVec4& v) noexcept : Bounds(v.x, v.y, v.w - v.x, v.z - v.y) {}
    operator ImVec4() const { return ImVec4(x, y, y + height, x + width); }
#endif
        
    explicit Bounds(const cv::Mat& matrix) : Bounds(0, 0, static_cast<Float2_t>(matrix.cols), static_cast<Float2_t>(matrix.rows)) {}
        
    template<typename T>
    Bounds(const cv::Rect_<T>& rect) : Bounds(rect.x, rect.y, rect.width, rect.height) {}
        
    template<typename T>
    operator cv::Rect_<T>() const { return cv::Rect_<T>((int)x, (int)y, (int)width, (int)height); }
        
    constexpr const Vec2 pos() const { return Vec2(x, y); }
    constexpr const Size2 size() const { return Size2(width, height); }
    constexpr void operator<<(const Size2& size) {
        width = size.width;
        height = size.height;
    }
    constexpr void operator<<(const Vec2& pos) {
        x = pos.x;
        y = pos.y;
    }
        
    constexpr void operator=(const Bounds& other) {
        x = other.x;
        y = other.y;
        width = other.width;
        height = other.height;
    }

    constexpr Bounds operator-(const Vec2& pos) {
        Bounds copy(*this);
        copy.x -= pos.x;
        copy.y -= pos.y;
        return copy;
    }
    constexpr Bounds operator+(const Vec2& pos) {
        Bounds copy(*this);
        copy.x += pos.x;
        copy.y += pos.y;
        return copy;
    }
    constexpr Bounds operator-(const Size2& size) {
        Bounds copy(*this);
        copy.width -= size.width;
        copy.height -= size.height;
        return copy;
    }
    constexpr Bounds operator+(const Size2& size) {
        Bounds copy(*this);
        copy.width += size.width;
        copy.height += size.height;
        return copy;
    }
        
    constexpr bool operator<(const Bounds& other) const {
        return pos() < other.pos() || (pos() == other.pos() && size() < other.size());
    }
        
    constexpr bool Equals(const Bounds& other) const {
        return pos().Equals(other.pos()) && size().Equals(other.size());
    }
    constexpr bool operator==(const Bounds& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
    constexpr bool operator!=(const Bounds& other) const {
        return x != other.x || y != other.y || width != other.width || height != other.height;
    }
        
    constexpr bool contains(const Vec2& point) const { return contains(point.x, point.y); }
    constexpr bool contains(const Float2_t x, const Float2_t y) const {
        return x >= this->x && x < this->x+width && y >= this->y && y < this->y+height;
    }
    constexpr bool contains(const Bounds& other) const {
        return contains(other.pos()) || contains(other.pos()+other.size());
    }
        
    void restrict_to(const Bounds& bounds);
    void insert_point(const Vec2& pt) {
        if(pt.x < x) x = pt.x;
        if(pt.y < y) y = pt.y;
        if(pt.x > width) width = pt.x;
        if(pt.y > height) height = pt.y;
    }
        
    template<typename V, bool K>
    constexpr Bounds mul(const Vector2D<V, K>& other) const {
        return Bounds(x * other.A(), y * other.B(), width * other.A(), height * other.B());
    }
        
    //! Calculate the bounding-box of combined this and other
    constexpr void combine(const Bounds& other) {
        // dont combine empty rects (for example Lines, which dont support bounds)
        if(other.width == 0 && other.height == 0)
            return;
            
        if(other.x < x) {
            if(x != FLT_MAX)
                width += x - other.x;
            x = other.x;
        }
            
        if(other.y < y) {
            if(y != FLT_MAX)
                height += y - other.y;
            y = other.y;
        }
            
        width  = max(x + width,  other.x+other.width) - x;
        height = max(y + height, other.y+other.height) - y;
    }
    
    static Bounds combine_all(const std::function<std::optional<Bounds>()>& iterate) {
        Bounds bds(std::numeric_limits<Float2_t>::max(), std::numeric_limits<Float2_t>::max(), 0, 0);
        while(true) {
            auto b = iterate();
            if(not b.has_value())
                break;
            bds.combine(b.value());
        }
        return bds;
    }
        
    constexpr bool overlaps(const Bounds& other) const {
        const auto r = x + width, oR = other.x + other.width;
        const auto b = y + height, oB = other.y + other.height;
            
        return (oR >= x && other.x <= r && oB >= y && other.y <= b)
            || (r >= other.x && x <= oR && b >= other.y && y <= oB);
    }
        
    constexpr bool intersects(const Bounds& other) const {
        return x < other.x + other.width  && x + width > other.x
            && y > other.y + other.height && y + height < other.y;
    }
        
    constexpr bool empty() const { return width == 0 && height == 0; }

    constexpr Vec2 bottom_right() const { return Vec2{ x + width, y + height }; }
    constexpr Vec2 center() const { return Vec2{ x + width * Float2_t(0.5), y + height * Float2_t(0.5) }; }
        
    Float2_t distance(const Vec2& p) const;
        
    std::string toStr() const {
        return "[" + Meta::toStr(x) + "," + Meta::toStr(y) + "," + Meta::toStr(width) + "," + Meta::toStr(height) + "]";
    }
    glz::json_t to_json() const {
        return glz::json_t{ x, y, width, height };
    }
    static Bounds fromStr(cmn::StringLike auto&& str)
    {
        auto vec = Meta::fromStr<std::vector<Float2_t>>(str);
        if(vec.empty())
            return Bounds();
        if(vec.size() != 4)
            throw std::invalid_argument("Can only initialize Bounds with exactly four or no elements. ('"+(std::string)str+"')");
        return Bounds(vec[0], vec[1], vec[2], vec[3]);
    }
    static std::string class_name() {
        return "bounds";
    }
};

    //static_assert(std::is_trivial_v<Vec2>, "Trivial type for Vec2.");
    //static_assert(std::is_trivial_v<Bounds>, "Trivial type for bounds.");
    
    //! Calculates the angle between two given vectors.
    //  The result has a value range from [0, pi] (acos) and thus does
    //  not provide information about signedness.
    inline Float2_t angle_between_vectors(const Vec2& v0, const Vec2& v1) {
        return acosf(v0.normalize().dot(v1.normalize()));
    }
        
    inline bool pnpoly(const std::vector<Vec2>& pts, const Vec2& pt)
    {
        size_t npol = pts.size();
        size_t i, j;
        bool c = false;
        for (i = 0, j = npol-1; i < npol; j = i++) {
            if ((((pts[i].y <= pt.y) && (pt.y < pts[j].y)) ||
                 ((pts[j].y <= pt.y) && (pt.y < pts[i].y))) &&
                (pt.x < (pts[j].x - pts[i].x) * (pt.y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x))
                c = !c;
        }
        return c;
    }
        
    inline std::shared_ptr<std::vector<Vec2>> poly_convex_hull(const std::vector<Vec2>* _vertices)
    {
        /**
            SOURCE: https://github.com/RandyGaul/ImpulseEngine
         
             Copyright (c) 2013 Randy Gaul http://RandyGaul.net

               This software is provided 'as-is', without any express or implied
               warranty. In no event will the authors be held liable for any damages
               arising from the use of this software.

               Permission is granted to anyone to use this software for any purpose,
               including commercial applications, and to alter it and redistribute it
               freely, subject to the following restrictions:
                 1. The origin of this software must not be misrepresented; you must not
                    claim that you wrote the original software. If you use this software
                    in a product, an acknowledgment in the product documentation would be
                    appreciated but is not required.
                 2. Altered source versions must be plainly marked as such, and must not be
                    misrepresented as being the original software.
                 3. This notice may not be removed or altered from any source distribution.
         */
        assert(_vertices);
        
        uint32_t count = (uint32_t)_vertices->size();
        if(count > 2 && count <= 64) {
            // No hulls with less than 3 vertices (ensure actual polygon)
            assert( count > 2 && count <= 64 );
            count = std::min( count, 64u );
            auto _points = std::make_shared<std::vector<Vec2>>();
            _points->resize(count);
            
            // Find the right most point on the hull
            uint32_t rightMost = 0;
            double highestXCoord = (*_vertices)[0].x;
            for(uint32_t i = 1; i < count; ++i)
            {
                double x = (*_vertices)[i].x;
                if(x > highestXCoord)
                {
                    highestXCoord = x;
                    rightMost = i;
                }
                
                // If matching x then take farthest negative y
                else if(x == highestXCoord)
                    if((*_vertices)[i].y < (*_vertices)[rightMost].y)
                        rightMost = i;
            }
            
            uint32_t hull[64];
            uint32_t outCount = 0;
            uint32_t indexHull = rightMost;
            
            for (;;)
            {
                hull[outCount] = indexHull;
                
                // Search for next index that wraps around the hull
                // by computing cross products to find the most counter-clockwise
                // vertex in the set, given the previos hull index
                uint32_t nextHullIndex = 0;
                for(uint32_t i = 1; i < count; ++i)
                {
                    // Skip if same coordinate as we need three unique
                    // points in the set to perform a cross product
                    if(nextHullIndex == indexHull)
                    {
                        nextHullIndex = i;
                        continue;
                    }
                    
                    // Cross every set of three unique vertices
                    // Record each counter clockwise third vertex and add
                    // to the output hull
                    // See : http://www.oocities.org/pcgpe/math2d.html
                    Vec2 e1 = (*_vertices)[nextHullIndex] - (*_vertices)[hull[outCount]];
                    Vec2 e2 = (*_vertices)[i] - (*_vertices)[hull[outCount]];
                    double c = cross( e1, e2 );
                    if(c < 0.0f)
                        nextHullIndex = i;
                    
                    // Cross product is zero then e vectors are on same line
                    // therefor want to record vertex farthest along that line
                    if(c == 0.0f && e2.sqlength() > e1.sqlength())
                        nextHullIndex = i;
                }
                
                ++outCount;
                indexHull = nextHullIndex;
                
                // Conclude algorithm upon wrap-around
                if(nextHullIndex == rightMost)
                {
                    _points->resize(outCount);
                    //m_vertexCount = outCount;
                    break;
                }
            }
            
            // Copy vertices into shape's vertices
            for(uint32_t i = 0; i < _points->size(); ++i)
                (*_points)[i] = (*_vertices)[hull[i]];
            
            return _points;
        }
        
        return nullptr;
    }

//! LineSegementsIntersect (modified a bunch) from https://www.codeproject.com/Tips/862988/Find-the-Intersection-Point-of-Two-Line-Segments under CPOL (https://www.codeproject.com/info/cpol10.aspx)
template<typename VecType>
inline std::optional<VecType> LineSegmentsIntersect(const VecType& p1, const VecType& p2, const VecType& q1, const VecType& q2)
{
    constexpr bool considerCollinearOverlapAsIntersect = false;
    using scalar = VecType::self_scalar;
    
    const auto r = p2 - p1;
    const auto s = q2 - q1;
    const auto r_cross_s = cross(r, s);
    const auto q1_minus_p1 = q1 - p1;
    const auto qp_cross_r = cross(q1_minus_p1, r);

    if (r_cross_s == 0 && qp_cross_r == 0)
    {
        if constexpr (considerCollinearOverlapAsIntersect)
        {
            const auto q_minus_p_dot_r = q1_minus_p1.dot(r);
            const auto p_minus_q_dot_s = (p1 - q1).dot(s);
            const auto t = q_minus_p_dot_r / r.dot(r);
            if ((0 <= q_minus_p_dot_r && q_minus_p_dot_r <= r.dot(r)) || (0 <= p_minus_q_dot_s && p_minus_q_dot_s <= s.dot(s)))
                return std::optional<VecType>(p1 + r * std::clamp(t, scalar(0.0), scalar(1.0)));
        }
        return std::nullopt;
    }

    if (r_cross_s == 0 && qp_cross_r != 0)
        return std::nullopt;

    const auto t = cross(q1_minus_p1, s) / r_cross_s;
    const auto u = cross(q1_minus_p1, r) / r_cross_s;

    if ((0 <= t && t < 1) && (0 <= u && u < 1))
        return std::optional<VecType>(p1 + t * r);

    return std::nullopt;
}

Float2_t polygon_area(const std::vector<Vec2>& pts);
}
