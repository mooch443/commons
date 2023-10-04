#pragma once

#include <misc/vec2.h>
#include <gui/types/Basic.h>

namespace gui {
namespace attr {

template <typename T>
inline constexpr bool pack_contains() {
    return false;
}

template <typename T, typename A, typename... Tail>
inline constexpr bool pack_contains() {
    return std::is_same<T, A>::value ? true : pack_contains<T, Tail...>();
}

template <class T, class Tag>
struct AttributeAlias : T {
    using base_type = T;
    using T::T;
    
    explicit constexpr AttributeAlias(const T& copy) : T(copy) {}
    explicit constexpr AttributeAlias(T&& copy) : T(std::move(copy)) {}
    constexpr AttributeAlias& operator=(const T& copy) {
        T::operator=(copy);
        return *this;
    }
    constexpr AttributeAlias& operator=(T&& copy) {
        T::operator=(std::move(copy));
        return *this;
    }
};

#define ATTRIBUTE_ALIAS(ALIAS_NAME, BASE_TYPE)                            \
    struct ALIAS_NAME##Tag {};                                  \
    using ALIAS_NAME = AttributeAlias<BASE_TYPE, ALIAS_NAME##Tag>;

// Using the ALIAS macro to define type aliases
ATTRIBUTE_ALIAS(Loc, cmn::Vec2)
ATTRIBUTE_ALIAS(Size, cmn::Size2)
ATTRIBUTE_ALIAS(SizeLimit, cmn::Size2)
ATTRIBUTE_ALIAS(Origin, cmn::Vec2)
ATTRIBUTE_ALIAS(Scale, cmn::Vec2)
ATTRIBUTE_ALIAS(Margins, cmn::Bounds)
ATTRIBUTE_ALIAS(Box, cmn::Bounds)
ATTRIBUTE_ALIAS(FillClr, gui::Color)
ATTRIBUTE_ALIAS(LineClr, gui::Color)
ATTRIBUTE_ALIAS(TextClr, gui::Color)
ATTRIBUTE_ALIAS(HighlightClr, gui::Color)
ATTRIBUTE_ALIAS(VerticalClr, gui::Color)
ATTRIBUTE_ALIAS(HorizontalClr, gui::Color)
ATTRIBUTE_ALIAS(Str, std::string)
ATTRIBUTE_ALIAS(Postfix, std::string)
ATTRIBUTE_ALIAS(Prefix, std::string)
ATTRIBUTE_ALIAS(ParmName, std::string)

template<typename T, int _arbitrary = 0>
struct NumberAlias {
    T value;
public:
    using self_type = NumberAlias<T, _arbitrary>;
    
public:
    constexpr self_type& operator++() { ++value; return *this; }
    constexpr self_type& operator--() { --value; return *this; }
    constexpr self_type operator++(int) { auto tmp = *this; ++*this; return tmp; }
    constexpr self_type operator--(int) { auto tmp = *this; --*this; return tmp; }
    constexpr self_type operator+() const { return *this; }
    constexpr self_type operator-() const { return self_type{-value}; }
    
    constexpr NumberAlias() = default;
    constexpr NumberAlias(const NumberAlias&) = default;
    constexpr NumberAlias(NumberAlias&&) = default;
    template<typename K>
        requires std::convertible_to<K, T>
    constexpr NumberAlias(K other) : value(other) {}
    constexpr self_type& operator=(const NumberAlias& other) = default;
    constexpr self_type& operator=(T other) { value = other; return *this; }
    
    /*
     With self type
     
    constexpr self_type& operator+=(const self_type& other) { value += other.value; return *this; }
    constexpr self_type& operator-=(const self_type& other) { value -= other.value; return *this; }
    constexpr self_type& operator*=(const self_type& other) { value *= other.value; return *this; }
    constexpr self_type& operator/=(const self_type& other) { value /= other.value; return *this; }
    
    constexpr self_type operator/(const self_type& other) const { return self_type{value / other.value}; }
    constexpr self_type operator*(const self_type& other) const { return self_type{value * other.value}; }
    constexpr self_type operator+(const self_type& other) const { return self_type{value + other.value}; }
    constexpr self_type operator-(const self_type& other) const { return self_type{value - other.value}; }*/
    
    constexpr operator T() const { return value; }
};

template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator+(const NumberAlias<T, a>& A, const K& B) {
    return NumberAlias<T, a>(A.value + B);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator-(const NumberAlias<T, a>& A, const K& B) {
    return NumberAlias<T, a>(A.value - B);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator/(const NumberAlias<T, a>& A, const K& B) {
    return NumberAlias<T, a>(A.value / B);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator*(const NumberAlias<T, a>& A, const K& B) {
    return NumberAlias<T, a>(A.value * B);
}

template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator+(const K& A, const NumberAlias<T, a>& B) {
    return NumberAlias<T, a>(A + B.value);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator-(const K& A, const NumberAlias<T, a>& B) {
    return NumberAlias<T, a>(A - B.value);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator*(const K& A, const NumberAlias<T, a>& B) {
    return NumberAlias<T, a>(A * B.value);
}
template<typename T, int a, typename K>
    requires std::integral<K> || std::floating_point<K>
constexpr NumberAlias<T, a> operator/(const K& A, const NumberAlias<T, a>& B) {
    return NumberAlias<T, a>(A / B.value);
}

using Radius = NumberAlias<float, 0>;
using Rotation = NumberAlias<float, 1>;
using Alpha = NumberAlias<float, 2>;
using Checked = NumberAlias<bool, 0>;
using Font = gui::Font;

}

}
