#pragma once

//#include <types.h>
#ifndef NDEBUG
#include <misc/metastring.h>
#endif

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
               )
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
inline constexpr To sign_cast(From&& value) {
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
constexpr bool check_narrow_cast(const From& value) {
#ifndef NDEBUG
    using FromType = typename cmn::remove_cvref<From>::type;
    using ToType = typename cmn::remove_cvref<To>::type;

    auto str = Meta::toStr(value);
    if constexpr (
        std::is_floating_point<ToType>::value
        || (std::is_signed<FromType>::value == std::is_signed<ToType>::value && !std::is_floating_point<FromType>::value)
        )
    {
        // unsigned to unsigned
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        FormatWarning("Narrowing ",tstr0.c_str()," -> ",tstr1.c_str()," (same) = ",str.c_str(),".");
#endif
        return true;
    }
    else if constexpr (std::is_floating_point<FromType>::value && std::is_signed<ToType>::value) {
        using signed_t = int64_t;
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        auto tstr2 = Meta::name<signed_t>();
        FormatWarning("Narrowing ",tstr0.c_str()," -> ",tstr1.c_str()," | converting to ",tstr2.c_str()," and comparing (fs) = ",str.c_str(),".");
#endif
        return static_cast<signed_t>(value) >= static_cast<signed_t>(std::numeric_limits<To>::min())
            && static_cast<signed_t>(value) <= static_cast<signed_t>(std::numeric_limits<To>::max());
    }
    else if constexpr (std::is_floating_point<FromType>::value && std::is_unsigned<ToType>::value) {
        using unsigned_t = uint64_t;
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        auto tstr2 = Meta::name<unsigned_t>();
        FormatWarning("Narrowing ",tstr0.c_str()," -> ",tstr1.c_str()," | converting to ",tstr2.c_str()," and comparing (fs) = ",str.c_str(),".");
#endif
        return value >= FromType(0)
            && static_cast<unsigned_t>(value) <= static_cast<unsigned_t>(std::numeric_limits<To>::max());
    }
    else if constexpr (std::is_unsigned<FromType>::value && std::is_signed<ToType>::value) {
        // unsigned to signed
        using signed_t = int64_t;
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        auto tstr2 = Meta::name<signed_t>();
        FormatWarning("Narrowing ",tstr0.c_str()," -> ",tstr1.c_str()," | converting to ",tstr2.c_str()," and comparing (us) = ",str.c_str(),".");
#endif
        return static_cast<signed_t>(value) < static_cast<signed_t>(std::numeric_limits<To>::max());

    }
    else {
        static_assert(std::is_signed<FromType>::value && std::is_unsigned<ToType>::value, "Expecting signed to unsigned conversion");
        // signed to unsigned
        using unsigned_t = typename try_make_unsigned<FromType>::type;
#ifdef _NARROW_PRINT_VERBOSE
        auto tstr0 = Meta::name<FromType>();
        auto tstr1 = Meta::name<ToType>();
        auto tstr2 = Meta::name<unsigned_t>();
        FormatWarning("Narrowing ",tstr0.c_str()," -> ",tstr1.c_str()," | converting to ",tstr2.c_str()," and comparing (su) = ",str.c_str(),".");
#endif
        return value >= 0 && static_cast<unsigned_t>(value) <= static_cast<unsigned_t>(std::numeric_limits<To>::max());
    }
#else
    UNUSED(value);
    return true;
#endif
}

template<typename To, typename From>
constexpr To narrow_cast(From&& value, struct tag::warn_on_error) {
#ifndef NDEBUG
    if (!check_narrow_cast<To, From>(value)) {
        auto vstr = Meta::toStr(value);
        auto lstr = Meta::toStr(std::numeric_limits<To>::min());
        auto rstr = Meta::toStr(std::numeric_limits<To>::max());

        auto tstr = Meta::name<To>();
        auto fstr = Meta::name<From>();
        FormatWarning("Value ",vstr," in narrowing conversion of ",fstr.c_str()," -> ",tstr.c_str()," is not within limits [",lstr.c_str(),",",rstr.c_str(),"].");
    }
#endif
    return static_cast<To>(std::forward<From>(value));
}

template<typename To, typename From>
constexpr To narrow_cast(From&& value, struct tag::fail_on_error) {
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
constexpr To narrow_cast(From&& value) {
    return narrow_cast<To, From>(std::forward<From>(value), tag::warn_on_error{});
}
}
