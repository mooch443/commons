#pragma once

#include <commons.pc.h>
#include <gui/types/Basic.h>
#include <gui/CornerFlags.h>

namespace cmn::gui {
namespace attr {

template <typename T>
inline constexpr bool pack_contains() {
    return false;
}

template <typename T, typename A, typename... Tail>
inline constexpr bool pack_contains() {
    return std::is_same<T, A>::value ? true : pack_contains<T, Tail...>();
}

// Concept to check if a type is Vec2 or Size2
template<typename T>
concept IsVec2OrSize2 = std::is_same_v<T, Vec2> || std::is_same_v<T, Size2>;

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
    
    // Delete the constructor for Vec2 or Size2
    template<typename U = T>
        requires are_the_same<U, Vector2D<Float2_t, false>>
    constexpr AttributeAlias(const Vector2D<Float2_t, true>& other) = delete;

    template<typename U = T>
        requires are_the_same<U, Vector2D<Float2_t, true>>
    constexpr AttributeAlias(const Vector2D<Float2_t, false>& other) = delete;
    
    // Delete constructors for other AttributeAlias types
    template <class OtherType, class OtherTag>
    //    requires IsVec2OrSize2<T>
    explicit constexpr AttributeAlias(const AttributeAlias<OtherType, OtherTag>&) = delete;

    template <class OtherType, class OtherTag>
    //    requires IsVec2OrSize2<T>
    explicit constexpr AttributeAlias(AttributeAlias<OtherType, OtherTag>&&) = delete;
    
    template <class OtherType, class OtherTag>
    //    requires IsVec2OrSize2<T>
    constexpr operator AttributeAlias<OtherType, OtherTag>() = delete;
    
    template<typename K = T>
        requires cmn::_has_fromstr_method<K>
    static AttributeAlias fromStr(const std::string& str) {
        return AttributeAlias{K::fromStr(str)};
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
ATTRIBUTE_ALIAS(CellFillClr, gui::Color)
ATTRIBUTE_ALIAS(CellLineClr, gui::Color)
ATTRIBUTE_ALIAS(Str, std::string)
ATTRIBUTE_ALIAS(Postfix, std::string)
ATTRIBUTE_ALIAS(Prefix, std::string)
ATTRIBUTE_ALIAS(ParmName, std::string)
ATTRIBUTE_ALIAS(Name, std::string)

ATTRIBUTE_ALIAS(CornerFlags_t, CornerFlags)
ATTRIBUTE_ALIAS(Placeholder_t, std::string)

// Macro for defining a NumberAlias
#define NUMBER_ALIAS(ALIAS_NAME, BASE_TYPE)                   \
    struct ALIAS_NAME##Tag {};                                \
    using ALIAS_NAME = NumberAlias<BASE_TYPE, ALIAS_NAME##Tag>;

template <typename T, class Tag>
struct NumberAlias {
    T value;
public:
    using self_type = NumberAlias<T, Tag>;

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
    explicit constexpr NumberAlias(K other) : value(other) {}
    constexpr self_type& operator=(const NumberAlias& other) = default;
    constexpr self_type& operator=(T other) { value = other; return *this; }
    
    constexpr operator T() const { return value; }
    
    static self_type fromStr(const std::string& str) { return self_type{ Meta::fromStr<T>(str) }; }
    std::string toStr() const { return Meta::toStr<T>(value); }
    static std::string class_name() { return type_name<self_type>(); }
};

template<typename T> struct _IsNumberAlias : std::false_type {};
template<typename T, class Tag> struct _IsNumberAlias<NumberAlias<T, Tag>> : std::true_type {};

template<typename T> concept IsNumberAlias = _IsNumberAlias<std::remove_cvref_t<T>>::value;

template<typename T, typename K, class Tag>
concept CompatibleArithmetic = std::integral<K> || std::floating_point<K> ||
                               std::same_as<NumberAlias<K, Tag>, T>;

#define DEFINE_ARITHMETIC_OP(OP) \
template<typename T, class Tag, typename K> \
    requires CompatibleArithmetic<T, K, Tag> \
constexpr NumberAlias<T, Tag> operator OP (const NumberAlias<T, Tag>& A, const K& B) { \
    if constexpr (std::is_same_v<NumberAlias<T, Tag>, K>) { \
        return NumberAlias<T, Tag>(A.value OP B.value); \
    } else { \
        return NumberAlias<T, Tag>(A.value OP B); \
    } \
} \
template<typename T, class Tag, typename K> \
    requires CompatibleArithmetic<T, K, Tag> \
constexpr NumberAlias<T, Tag> operator OP (const K& A, const NumberAlias<T, Tag>& B) { \
    if constexpr (std::is_same_v<NumberAlias<T, Tag>, K>) { \
        return NumberAlias<T, Tag>(A.value OP B.value); \
    } else { \
        return NumberAlias<T, Tag>(A OP B.value); \
    } \
}

// Define operators
DEFINE_ARITHMETIC_OP(+)
DEFINE_ARITHMETIC_OP(-)
DEFINE_ARITHMETIC_OP(*)
DEFINE_ARITHMETIC_OP(/)

#undef DEFINE_ARITHMETIC_OP

NUMBER_ALIAS(Radius, Float2_t)
NUMBER_ALIAS(Rotation, Float2_t)
NUMBER_ALIAS(Alpha, Float2_t)
NUMBER_ALIAS(Checked, bool)
NUMBER_ALIAS(Folded_t, bool)
NUMBER_ALIAS(Clickable, bool)
NUMBER_ALIAS(ZIndex, int)
using Font = gui::Font;

}

}
