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
using NumberAlias = T;

using Radius = NumberAlias<float, 0>;
using Rotation = NumberAlias<float, 1>;
using Alpha = NumberAlias<float, 2>;
using Font = gui::Font;

}

}
