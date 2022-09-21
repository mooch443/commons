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

template <class T, int _position = 0>
struct AttributeAlias : T {
    using base_type = T;
    using base_type::base_type;
    //static constexpr auto position = _position;
    
    constexpr AttributeAlias(const T& copy) : T(copy) {}
    constexpr AttributeAlias(T&& copy) : T(std::move(copy)) {}
    constexpr AttributeAlias& operator=(const T& copy) { T::operator=(copy); return *this; }
    constexpr AttributeAlias& operator=(T&& copy) { T::operator=(std::move(copy)); return *this; }
};

using Loc = AttributeAlias<cmn::Vec2, 0>;
using Size = AttributeAlias<cmn::Size2, 0>;
using SizeLimit = AttributeAlias<cmn::Size2, 1>;
using Origin = AttributeAlias<cmn::Vec2, 1>;
using Scale = AttributeAlias<cmn::Vec2, 2>;
using Margins = AttributeAlias<cmn::Vec2, 3>;
using FillClr = AttributeAlias<gui::Color, 0>;
using LineClr = AttributeAlias<gui::Color, 1>;
using TextClr = AttributeAlias<gui::Color, 2>;
using BgClr = AttributeAlias<gui::Color, 3>;
using Content = AttributeAlias<std::string, 1>;
using Postfix = AttributeAlias<std::string, 2>;
using Prefix = AttributeAlias<std::string, 3>;

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
using Font = gui::Font;

}

}
