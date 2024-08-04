#pragma once
#include <commons.pc.h>

namespace cmn {
namespace tag {
struct warn_on_error {};
struct fail_on_error {};
}

template<typename T>
using try_make_unsigned =
typename std::conditional<
    std::is_integral<T>::value,
    std::make_unsigned<T>,
    double
>::type;

template<typename T>
using try_make_signed =
typename std::conditional<
    std::is_integral<T>::value,
    std::make_signed<T>,
    double
>::type;

template<typename To, typename From>
void fail_type(From&&
#ifndef NDEBUG
               value
#endif
               ) noexcept
{
#ifndef NDEBUG
    using FromType = typename cmn::remove_cvref<From>::type;
    using ToType = typename cmn::remove_cvref<To>::type;
    
    auto type1 = Meta::name<FromType>();
    auto type2 = Meta::name<ToType>();
    
    auto value1 = Meta::toStr(value);
    
    auto start1 = Meta::toStr(std::numeric_limits<FromType>::min());
    auto end1 = Meta::toStr(std::numeric_limits<FromType>::max());
    
    auto start2 = Meta::toStr(std::numeric_limits<ToType>::min());
    auto end2 = Meta::toStr(std::numeric_limits<ToType>::max());
    
    FormatWarning("Failed converting ", type1.c_str(),"(", value1.c_str(),") [", start1.c_str(),",", end1.c_str(),"] -> type ", type2.c_str()," [", start2.c_str(),",", end2.c_str(),"]");
#endif
}

template<typename To, typename From>
inline constexpr To sign_cast(From&& value)
#ifdef NDEBUG
noexcept
#endif
{
#ifndef NDEBUG
    using FromType = typename cmn::remove_cvref<From>::type;
    using ToType = typename cmn::remove_cvref<To>::type;
    
    if constexpr(std::is_integral<ToType>::value)
    {
        if constexpr(std::is_signed<ToType>::value) {
            if (value > std::numeric_limits<ToType>::max())
                fail_type<To, From>(std::forward<From>(value));
            
        } else if constexpr(std::is_signed<FromType>::value) {
            if (value < 0)
                fail_type<To, From>(std::forward<From>(value));
            
            using bigger_type = typename std::conditional<(sizeof(FromType) > sizeof(ToType)), FromType, ToType>::type;
            if (bigger_type(value) > bigger_type(std::numeric_limits<ToType>::max()))
                fail_type<To, From>(std::forward<From>(value));
        }
    }
#endif
    return static_cast<To>(std::forward<From>(value));
}

template<typename To, typename From>
constexpr bool check_narrow_cast(const From& value) noexcept {
#ifndef NDEBUG
    using FromType = typename cmn::remove_cvref<From>::type;
    using ToType = typename cmn::remove_cvref<To>::type;
#ifdef _NARROW_PRINT_VERBOSE
    auto str = Meta::toStr(value);
#endif
    //Print("Checking whether ", value, " can be narrowed to ", Meta::name<ToType>(), " from ", Meta::name<FromType>(), ".");
    if constexpr (std::is_floating_point<ToType>::value) {
        // For floating-point targets, the range check isn't typically necessary for integers,
        // but you might want to check for extremely large values for completeness.
        if constexpr (std::is_floating_point<FromType>::value) {
            // Handling floating-point to floating-point conversions
            if constexpr (sizeof(FromType) > sizeof(ToType)) {
                // Check for overflow and underflow
                if (!std::isfinite(value) 
                    || std::isnan(value)
                    || value > std::numeric_limits<ToType>::max()
                    || value < std::numeric_limits<ToType>::lowest()) 
                {
                    return false;
                }

                // Check for precision loss
                constexpr FromType epsilon = std::numeric_limits<FromType>::epsilon();
                ToType converted = static_cast<ToType>(value);
                FromType backConverted = static_cast<FromType>(converted);
                FromType difference = std::abs(value - backConverted);
                return value == backConverted || difference <= epsilon;
            }
            return true;  // No narrowing issues for conversions to larger or same size types
        }
        // Here, we'll simply return true to indicate that typical integer values are fine.
        return true;
    } else if constexpr (std::is_signed<FromType>::value == std::is_signed<ToType>::value
                         && !std::is_floating_point<FromType>::value)
    {
        // Print verbose output if enabled
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ", tstr0.c_str(), " -> ", tstr1.c_str(), " (same) = ", str.c_str(), ".");
#endif
        if constexpr(std::is_signed_v<ToType> || sizeof(FromType) > sizeof(ToType))
                return value >= std::numeric_limits<ToType>::min()
                    && value <= std::numeric_limits<ToType>::max();
            else
                return true; // No narrowing issues for unsigned types in this context
    }
    // Check if the conversion is from floating-point to signed integer type
    else if constexpr (std::is_floating_point<FromType>::value && std::is_signed<ToType>::value) {
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ", tstr0.c_str(), " -> ", tstr1.c_str(), " | converting to int64_t and comparing (fs) = ", str.c_str(), ".");
#endif
        return value >= static_cast<FromType>(std::numeric_limits<ToType>::min())
            && value <= static_cast<FromType>(std::numeric_limits<ToType>::max());
    }
    // Check if the conversion is from floating-point to unsigned integer type
    else if constexpr (std::is_floating_point<FromType>::value && std::is_unsigned<ToType>::value) {
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ", tstr0.c_str(), " -> ", tstr1.c_str(), " | converting to uint64_t and comparing (fu) = ", str.c_str(), ".");
#endif
        return value >= FromType(0)
            && value <= static_cast<FromType>(std::numeric_limits<ToType>::max());
    }
    // Check if the conversion is from unsigned integer to signed integer type
    else if constexpr (std::is_unsigned<FromType>::value && std::is_signed<ToType>::value) {
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ", tstr0.c_str(), " -> ", tstr1.c_str(), " | converting to int64_t and comparing (us) = ", str.c_str(), ".");
#endif
        return value < static_cast<typename std::make_unsigned<ToType>::type>(std::numeric_limits<ToType>::max());
    }
    else {
        // Expecting signed integer to unsigned integer type conversion
        static_assert(std::is_signed<FromType>::value && std::is_unsigned<ToType>::value, "Expecting signed to unsigned conversion");
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ", tstr0.c_str(), " -> ", tstr1.c_str(), " | converting to uint64_t and comparing (su) = ", str.c_str(), ".");
#endif
        return value >= FromType(0)
            && static_cast<typename std::make_unsigned<FromType>::type>(value) <= std::numeric_limits<ToType>::max();
    }
#else
    // In release mode, don't perform the checks
    UNUSED(value);
    return true;
#endif
}

template<typename To, typename From>
constexpr To narrow_cast(From&& value, struct tag::warn_on_error) noexcept {
#ifndef NDEBUG
    if (!check_narrow_cast<To, From>(value)) {
        auto vstr = Meta::toStr(value);
        auto lstr = Meta::toStr(std::numeric_limits<To>::min());
        auto rstr = Meta::toStr(std::numeric_limits<To>::max());

        auto tstr = Meta::name<To>();
        auto fstr = Meta::name<From>();
        auto result = check_narrow_cast<To, From>(value);
        FormatWarning("Value ",vstr," in narrowing conversion of ",fstr.c_str()," -> ",tstr.c_str()," is not within limits [",lstr.c_str(),",",rstr.c_str(),"].", result);
    }
#endif
    return static_cast<To>(std::forward<From>(value));
}

template<typename To, typename From>
constexpr To narrow_cast(From&& value, struct tag::fail_on_error)
#ifdef NDEBUG
noexcept
#endif
{
#ifndef NDEBUG
    if (!check_narrow_cast<To, From>(value)) {
        auto vstr = Meta::toStr(value);
        auto lstr = Meta::toStr(std::numeric_limits<To>::min());
        auto rstr = Meta::toStr(std::numeric_limits<To>::max());

        auto tstr = Meta::name<To>();
        auto fstr = Meta::name<From>();
        throw U_EXCEPTION("Value ",vstr," in narrowing conversion of ",fstr.c_str()," -> ",tstr.c_str()," is not within limits [",lstr.c_str(),",",rstr.c_str(),"].");
    }
#endif
    return static_cast<To>(std::forward<From>(value));
}

template<typename To, typename From>
constexpr To narrow_cast(From&& value) noexcept {
    return narrow_cast<To, From>(std::forward<From>(value), tag::warn_on_error{});
}
}
