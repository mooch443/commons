#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#ifdef __llvm__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Wgcc-compat"
#pragma clang diagnostic ignored "-Wc11-extensions"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#endif

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h> // e.g. istty
#endif
#include <ctime>
#include <iomanip>
#include <locale>
#include <type_traits>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <functional>
#include <memory>
#include <chrono>
#include <set>
#include <unordered_set>
#include <queue>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <ostream>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdarg.h>
#include <concepts>
#include <fstream>
#include <span>

#include <misc/date.h>

#ifdef WIN32
//#define _USE_MATH_DEFINES
#include <cmath>
#include <intrin.h>
#include <cstdint>

#if defined(_MSC_VER)
static inline int __builtin_clz(unsigned x) {
    return (int)__lzcnt(x);
}

static inline int __builtin_clzll(unsigned long long x) {
    return (int)__lzcnt64(x);
}

template<typename T>
static inline int __builtin_clzl(T x) {
    if constexpr (sizeof(x) == sizeof(uint64_t)) {
        return __builtin_clzll(x);
    }
    else {
        return __builtin_clz(x);
        }
    }
#endif

#endif

#if defined(__clang__)
    #if (__clang_major__) <= 13

namespace std {

template<class From, class To>
    concept convertible_to =
        std::is_convertible_v<From, To> &&
        requires {
            static_cast<To>(std::declval<From>());
        };
template<class T>
    concept floating_point = std::is_floating_point_v<T>;
template<class T>
    concept integral = std::is_integral_v<T>;
template <class T>
    concept signed_integral = std::integral<T> && std::is_signed_v<T>;
template <class T>
    concept unsigned_integral = std::integral<T> && !std::signed_integral<T>;
}

    #endif
#endif

#ifdef WIN32
#define _USE_MATH_DEFINES
#include <cmath>
#endif

using long_t = int32_t;

#ifdef __cpp_lib_source_location
#include <source_location>
namespace cmn {
using source_location = std::source_location;
}
#else
namespace cmn {

class source_location {
    const char* _file_name;
    const char* _function_name;
    int _line;

    constexpr source_location(const char* file, const char* func, int line) noexcept
        : _file_name(file)
        , _function_name(func)
        , _line(line)
    { }
    
public:
    constexpr source_location() noexcept = default;
    source_location(const source_location& other) = default;
    source_location(source_location&& other) = default;

    static source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        int line = __builtin_LINE()) noexcept
    {
        return source_location(file, func, line);
    }

    constexpr const char* file_name() const noexcept { return _file_name; }
    constexpr const char* function_name() const noexcept { return _function_name; }
    constexpr int line() const noexcept { return _line; }
};

}
#endif

#ifndef _MSC_VER
#define CV_STATIC_ANALYSIS 0
#define CV_ErrorNoReturn(code, msg) cv::errorNoReturn( code, msg, CV_Func, __FILE__, __LINE__ )
#endif

#include <opencv2/opencv.hpp>
#include <cnpy/cnpy.h>
#include <bytell_hash_map.hpp>
//#include <tsl/sparse_map.h>
//#include <tsl/sparse_set.h>
#include <robin_hood.h>


#ifndef CMN_WITH_IMGUI_INSTALLED
    #if __has_include ( <imgui/imgui.h> )
    #include <imgui/imgui.h>
        #define CMN_WITH_IMGUI_INSTALLED true
    #else
        #define CMN_WITH_IMGUI_INSTALLED false
    #endif
#endif

#ifdef __llvm__
#pragma clang diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef WIN32
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef TEXT
#undef TEXT
#endif
#ifdef small
#undef small
#endif

#define __attribute__(X)
#endif

#if CV_MAJOR_VERSION >= 3
#define USE_GPU_MAT
#else
static_assert(false, "OpenCV version insufficient.");
#endif

namespace cmn {

#ifndef __cpp_lib_remove_cvref
template< class T >
struct remove_cvref {
    typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};

template< class T >
using remove_cvref_t = typename remove_cvref<T>::type;
#else
template< class T >
using remove_cvref = std::remove_cvref<T>;
template< class T >
using remove_cvref_t = std::remove_cvref_t<T>;
#endif

#if __has_cpp_attribute(deprecated)
#define DEPRECATED [[deprecated]]
#else
#define DEPRECATED
#endif
    
#ifdef USE_GPU_MAT
    typedef cv::UMat gpuMat;
#else
    typedef cv::Mat gpuMat;
#endif
    
    typedef float ScalarType;
    
    typedef cv::Matx<ScalarType, 1, 3> Mat13;
    
    typedef cv::Matx<ScalarType, 3, 4> Mat34;
    typedef cv::Matx<ScalarType, 3, 3> Mat33;
    typedef cv::Matx<ScalarType, 5, 1> Mat51;
    typedef cv::Matx<ScalarType, 4, 1> Mat41;
    typedef cv::Matx<ScalarType, 3, 1> Mat31;
    typedef cv::Matx<ScalarType, 2, 1> Mat21;
    typedef cv::Matx<ScalarType, 4, 4> Mat44;
    
#define DEGREE(radians) ((radians) * (cmn::ScalarType(1.0) / cmn::ScalarType(M_PI) * cmn::ScalarType(180)))
#define RADIANS(degree) ((degree) * (cmn::ScalarType(1.0) / cmn::ScalarType(180) * cmn::ScalarType(M_PI)))
#define SQR(X) ((X)*(X))
    
#define GETTER_CONST(TYPE, VAR) protected: TYPE _##VAR; public: inline TYPE& VAR() const { return _##VAR; } protected:
#define GETTER(TYPE, VAR) protected: TYPE _##VAR; public: const TYPE& VAR() const { return _##VAR; } protected:
#define GETTER_I(TYPE, VAR, INIT) protected: TYPE _##VAR = INIT; public: const TYPE& VAR() const { return _##VAR; } protected:
#define GETTER_NCONST(TYPE, VAR) protected: TYPE _##VAR; public: inline const TYPE& VAR() const { return _##VAR; } inline TYPE& VAR() { return _##VAR; } protected:
#define GETTER_PTR(TYPE, VAR) protected: TYPE _##VAR; public: inline TYPE VAR() const { return _##VAR; } inline TYPE & VAR() { return _##VAR; } protected:
#define GETTER_PTR_I(TYPE, VAR, INIT) protected: TYPE _##VAR = INIT; public: inline TYPE VAR() const { return _##VAR; } inline TYPE & VAR() { return _##VAR; } protected:
#define GETTER_CONST_PTR(TYPE, VAR) protected: TYPE _##VAR; public: inline const TYPE VAR() const { return _##VAR; } protected:
#define GETTER_NREF(TYPE, VAR) protected: TYPE _##VAR; public: inline const TYPE VAR() const { return _##VAR; } inline TYPE VAR() { return _##VAR; } protected:
#define GETTER_SETTER(TYPE, VAR) protected: TYPE _##VAR; public: inline const TYPE& VAR() const { return _##VAR; } inline void set_##VAR(const TYPE& value) { _##VAR = value; } protected:
#define GETTER_SETTER_I(TYPE, VAR, INIT) protected: TYPE _##VAR = INIT; public: inline const TYPE& VAR() const { return _##VAR; } inline void set_##VAR(const TYPE& value) { _##VAR = value; } protected:
#define GETTER_SETTER_PTR(TYPE, VAR) protected: TYPE _##VAR; public: inline TYPE VAR() const { return _##VAR; } inline void set_##VAR(TYPE value) { _##VAR = value; } protected:
#define GETTER_SETTER_PTR_I(TYPE, VAR, INIT) protected: TYPE _##VAR = INIT; public: inline TYPE VAR() const { return _##VAR; } inline void set_##VAR(TYPE value) { _##VAR = value; } protected:
#define IMPLEMENT(VAR) decltype( VAR ) VAR

    template<typename T0, typename T1,
        typename T0_ = typename cmn::remove_cvref<T0>::type,
        typename T1_ = typename cmn::remove_cvref<T1>::type,
        typename Result = typename std::conditional<(sizeof(T0_) > sizeof(T1_)), T0_, T1_>::type >
    constexpr inline auto min(T0&& x, T1&& y)
        -> typename std::enable_if< std::is_signed<T0_>::value == std::is_signed<T1_>::value
                                && (std::is_integral<T0_>::value || std::is_floating_point<T0_>::value)
                                && (std::is_integral<T1_>::value || std::is_floating_point<T1_>::value), Result>::type
    {
        return std::min(Result(x), Result(y));
    }
    
    template<typename T0, typename T1>
    constexpr inline auto min(const T0& x, const T1& y)
        -> typename std::enable_if<std::is_same<decltype(x.x), decltype(x.y)>::value, T0>::type
    {
        return T0(std::min(decltype(x.x+y.x)(x.x), decltype(x.x+y.x)(y.x)),
                  std::min(decltype(x.y+y.y)(x.y), decltype(x.y+y.y)(y.y)));
    }
    
    template<typename T0, typename T1>
    constexpr inline T0 min(const T0& x, const T1& y, typename std::enable_if<std::is_same<decltype(x.width), decltype(x.height)>::value, bool>::type * =NULL) {
        return T0(std::min(decltype(x.width+y.width)(x.width), decltype(x.width+y.width)(y.width)),
                  std::min(decltype(x.height+y.height)(x.height), decltype(x.height+y.height)(y.height)));
    }
    
    template<typename T0, typename T1, typename T2>
    constexpr inline auto min(const T0& x, const T1& y, const T2& z) -> decltype(x+y+z) {
        return std::min(decltype(x+y+z)(x), std::min(decltype(x+y+z)(y), decltype(x+y+z)(z)));
    }
        
    template<typename T0, typename T1,
        typename T0_ = typename cmn::remove_cvref<T0>::type,
        typename T1_ = typename cmn::remove_cvref<T1>::type,
        typename Result = typename std::conditional<(sizeof(T0_) > sizeof(T1_)), T0_, T1_>::type >
    constexpr inline auto max(T0&& x, T1&& y)
        -> typename std::enable_if< std::is_signed<T0_>::value == std::is_signed<T1_>::value
                                && (std::is_integral<T0_>::value || std::is_floating_point<T0_>::value)
                                && (std::is_integral<T1_>::value || std::is_floating_point<T1_>::value), Result>::type
    {
        return std::max(Result(x), Result(y));
    }
    
    template<typename T0, typename T1>
    constexpr inline auto max(const T0& x, const T1& y)
        -> typename std::enable_if<std::is_same<decltype(x.x), decltype(x.y)>::value, T0>::type
    {
        return T0(std::max(decltype(x.x+y.x)(x.x), decltype(x.x+y.x)(y.x)),
                  std::max(decltype(x.y+y.y)(x.y), decltype(x.y+y.y)(y.y)));
    }
    
    template<typename T0, typename T1>
    constexpr inline T0 max(const T0& x, const T1& y, typename std::enable_if<std::is_same<decltype(x.width), decltype(y.height)>::value, bool>::type * =NULL) {
        return T0(std::max(decltype(x.width+y.width)(x.width), decltype(x.width+y.width)(y.width)),
                  std::max(decltype(x.height+y.height)(x.height), decltype(x.height+y.height)(y.height)));
    }
    
    template<typename T0, typename T1>
    constexpr inline T0 max(const T0& x, const T1& y, typename std::enable_if<std::is_same<decltype(x.width), decltype(y.y)>::value, bool>::type * =NULL) {
        return T0(std::max(decltype(x.width+y.x)(x.width), decltype(x.width+y.x)(y.x)),
                  std::max(decltype(x.height+y.y)(x.height), decltype(x.height+y.y)(y.y)));
    }

    template<typename T0, typename T1, typename T2>
    constexpr inline auto max(const T0& x, const T1& y, const T2& z) -> decltype(x+y+z) {
        return std::max(decltype(x+y+z)(x), std::max(decltype(x+y+z)(y), decltype(x+y+z)(z)));
    }
    
    /**
     * ======================
     * COMMON custom STL selectors and iterators
     * ======================
     */
    
    /*template<typename Func, typename T>
    void foreach(Func callback, std::vector<T> &v) {
        std::for_each(v.begin(), v.end(), callback);
    }
    
    template<typename Func, typename T, typename... Args>
    void foreach(Func callback, std::vector<T> &v, Args... args) {
        std::for_each(v.begin(), v.end(), callback);
        return foreach(callback, args...);
    }*/
    
    template<typename Func, typename T>
    void foreach(Func callback, T &v) {
        std::for_each(v.begin(), v.end(), callback);
    }
    
    template<typename Func, typename T, typename... Args>
    void foreach(Func callback, T &v, Args... args) {
        std::for_each(v.begin(), v.end(), callback);
        return foreach(callback, args...);
    }


    template<typename T>
    struct UnorderedVectorSet {
        using iterator = typename std::vector<T>::iterator;
        using const_iterator = typename std::vector<T>::const_iterator;

        std::vector<T> _data;

        UnorderedVectorSet() = default;
        UnorderedVectorSet(const std::initializer_list<T>& init) : _data(init) {}
        UnorderedVectorSet(UnorderedVectorSet&&) = default;
        UnorderedVectorSet(const UnorderedVectorSet&) = default;
        UnorderedVectorSet(std::vector<T>&& vic) : _data(std::move(vic)) {}

        UnorderedVectorSet& operator=(const UnorderedVectorSet&) = default;

        template<typename K>
            requires std::convertible_to<K, T>
        void insert(const K& value) {
            if (!this->contains(value))
                _data.emplace_back(value);
        }

        template<typename K>
            requires std::convertible_to<K, T>
        bool contains(const K& value) const {
            return std::find(_data.begin(), _data.end(), value) != _data.end();
        }

        template<typename K>
            requires std::convertible_to<K, T>
        auto find(const K& value) const {
            return std::find(_data.begin(), _data.end(), value);
        }

        void clear() {
            _data.clear();
        }

        template<typename Iterator>
        void insert(Iterator start, Iterator end) {
            for (auto it = start; it != end; ++it)
                insert(*it);
        }
        
        template<typename Iterator>
        inline auto erase(Iterator it) {
            return _data.erase(it);
        }
        
        template<class... Args>
        constexpr typename std::vector<T>::reference emplace(Args&&... args) {
            return _data.emplace_back(std::forward<Args>(args)...);
        }

        auto begin() { return _data.begin(); }
        auto end() { return _data.end(); }
        auto begin() const { return _data.begin(); }
        auto end() const { return _data.end(); }
        size_t size() const { return _data.size(); }
        bool empty() const { return _data.empty(); }
    };
    
    template<class T> struct is_container : public std::false_type {};
    
    template<class T, class Alloc>
    struct is_container<std::vector<T, Alloc>> : public std::true_type {};
    
    template<class T, size_t Size>
    struct is_container<std::array<T, Size>> : public std::true_type {};

    template<class T, size_t Size>
    struct is_container<std::span<T, Size>> : public std::true_type {};
    
    template<class T> struct is_set : public std::false_type {};
    template<class T, class Alloc>
    struct is_set<std::set<T, Alloc>> : public std::true_type {};
    template<class T, class Alloc>
    struct is_set<std::multiset<T, Alloc>> : public std::true_type {};
    template<class T, class Alloc>
    struct is_set<std::unordered_set<T, Alloc>> : public std::true_type {};
    template<class T>
    struct is_set<ska::bytell_hash_set<T>> : public std::true_type {};
    template<class T, class Alloc>
    struct is_set<robin_hood::unordered_set<T, Alloc>> : public std::true_type {};
    template<class T, class Alloc>
    struct is_set<robin_hood::unordered_flat_set<T, Alloc>> : public std::true_type {};
   // template<class T, class Alloc>
    //struct is_set<tsl::sparse_set<T, Alloc>> : public std::true_type {};
    template<class T>
    struct is_set<UnorderedVectorSet<T>> : public std::true_type {};
    
    template<class T> struct is_map : public std::false_type {};
    template<class T, class Compare, class Alloc>
    struct is_map<std::map<T, Compare, Alloc>> : public std::true_type {};
    template<class T, class Compare, class Alloc>
    struct is_map<std::unordered_map<T, Compare, Alloc>> : public std::true_type {};
    template<class T, class Compare, class Alloc>
    struct is_map<ska::bytell_hash_map<T, Compare, Alloc>> : public std::true_type {};
    //template<class T, class Compare, class Alloc>
    //struct is_map<robin_hood::unordered_node_map<T, Compare, Alloc>> : public std::true_type {};
    //template<class T, class Compare, class Alloc>
    //struct is_map<robin_hood::unordered_flat_map<T, Compare, Alloc>> : public std::true_type {};
    template<bool T, size_t S, class... Args>
    struct is_map<robin_hood::detail::Table<T, S, Args...>> : public std::true_type {};

    template<class T> struct is_queue : public std::false_type {};
    template<class T, class Container>
    struct is_queue<std::queue<T, Container>> : public std::true_type {};
    template<class T, class Allocator>
    struct is_queue<std::deque<T, Allocator>> : public std::true_type {};

    template<class T> struct is_deque : public std::false_type {};
    template<class T, class Allocator>
    struct is_deque<std::deque<T, Allocator>> : public std::true_type {};
    
    template< class T >
    struct is_pair : public std::false_type {};
    
    template< class T1 , class T2 >
    struct is_pair< std::pair< T1 , T2 > > : public std::true_type {};
    
    class IndexedDataTransport {
    protected:
        GETTER_SETTER(long_t, index)
        
    public:
        virtual ~IndexedDataTransport() {}
    };
    
    template<typename T>
    typename T::value_type percentile(const T& values, float percent, typename std::enable_if<is_set<T>::value || is_container<T>::value, T>::type* = NULL)
    {
        using C = typename std::conditional<std::is_floating_point<typename T::value_type>::value,
            typename T::value_type, double>::type;
        
        auto start = values.begin();
        
        if(values.empty())
            return std::numeric_limits<typename T::value_type>::max();
        
        C stride = C(values.size()-1) * percent;
        std::advance(start, (int64_t)stride);
        C A = *start;
        
        auto second = start;
        ++second;
        if(second != values.end())
            start = second;
        C B = *start;
        C p = stride - C(size_t(stride));
        
        return A * (1 - p) + B * p;
    }
    
    template<typename T, typename K>
    std::vector<typename T::value_type> percentile(const T& values, const std::initializer_list<K>& tests, typename std::enable_if<(is_set<T>::value || is_container<T>::value) && std::is_floating_point<typename T::value_type>::value, T>::type* = NULL)
    {
        std::vector<typename T::value_type> result;
        for(auto percent : tests)
            result.push_back(percentile(values, percent));
        
        return result;
    }

    template<template<class...>class Template, class T>
    struct is_instantiation : std::false_type {};
    template<template<class...>class Template, class... Ts>
    struct is_instantiation<Template, Template<Ts...>> : std::true_type {};

    template<typename T, typename Q>
        requires (is_container<T>::value || is_queue<T>::value || is_deque<T>::value) && (!is_instantiation<UnorderedVectorSet, T>::value)
    inline bool contains(const T& s, const Q& value) {
        return std::find(s.begin(), s.end(), value) != s.end();
    }

    template<typename T, typename Q>
        requires is_instantiation<UnorderedVectorSet, T>::value
    inline bool contains(const T& s, const Q& value) {
        return s.contains(value);
    }

    template<typename T, typename K>
        requires std::convertible_to<K, T>
    inline bool contains(const ska::bytell_hash_set<T>& s, const K& value) {
        return s.count(value) > 0;
    }

    template<typename T, typename K>
        requires std::convertible_to<K, T>
    inline bool contains(const robin_hood::unordered_flat_set<T>& s, const K& value) {
        return s.contains(value);
    }

    /*template<typename T, typename K>
        requires std::convertible_to<K, T>
    inline bool contains(const tsl::sparse_set<T>& s, const K& value) {
        return s.contains(value);
    }*/

    /*template<typename T, typename Q>
    inline bool contains(const Q& v, const T& obj) {
        return std::find(v.begin(), v.end(), obj) != v.end();
    }*/

    template<typename T>
    inline bool contains(const std::set<T>& v, T obj) {
        //static_assert(!std::is_same<T, typename decltype(v)::value_type>::value, "We should not use this for sets
#if __cplusplus >= 202002L
        return v.contains(obj);
#else
        return v.find(obj) != v.end();
#endif
    }

    template<typename T, typename V>
    inline bool contains(const std::map<T, V>& v, T key) {
        //static_assert(!std::is_same<T, typename decltype(v)::value_type>::value, "We should not use this for sets.");
#if __cplusplus >= 202002L
        return v.contains(key);
#else
        return v.find(key) != v.end();
#endif
    }
}

namespace cmn {
struct timestamp_t {
    using value_type = uint64_t;
    std::optional<value_type> value;
    
    timestamp_t() = default;
    timestamp_t(const timestamp_t&) = default;
    timestamp_t(timestamp_t&&) = default;
    
    constexpr timestamp_t(value_type v) : value(v) {}
    template<typename Rep, typename C>
    explicit timestamp_t(const std::chrono::duration<Rep, C>& duration)
        : value(std::chrono::duration_cast<std::chrono::microseconds>(duration).count())
    {}
    //constexpr timestamp_t(double v) : value(v) {}
    
    constexpr timestamp_t& operator=(const timestamp_t&) = default;
    constexpr timestamp_t& operator=(timestamp_t&&) = default;
    
    explicit constexpr operator value_type() const { return get(); }
    explicit constexpr operator double() const { return double(get()); }
#ifndef NDEBUG
    constexpr value_type get(cmn::source_location loc = cmn::source_location::current()) const {
        if(!valid())
            throw std::invalid_argument("Invalid access to "+std::string(loc.file_name())+":"+std::to_string(loc.line())+".");
        return value.value();
    }
#else
    constexpr value_type get() const { return value.value(); }
#endif
    
    constexpr bool valid() const { return value.has_value(); }
    
    constexpr bool operator==(const timestamp_t& other) const {
        return other.get() == get();
    }
    constexpr bool operator!=(const timestamp_t& other) const {
        return other.get() != get();
    }
    
    constexpr bool operator<(const timestamp_t& other) const {
        return get() < other.get();
    }
    constexpr bool operator>(const timestamp_t& other) const {
        return get() > other.get();
    }
    
    constexpr bool operator<=(const timestamp_t& other) const {
        return get() <= other.get();
    }
    constexpr bool operator>=(const timestamp_t& other) const {
        return get() >= other.get();
    }
    
    constexpr timestamp_t operator-(const timestamp_t& other) const {
        return timestamp_t(get() - other.get());
    }
    constexpr timestamp_t operator+(const timestamp_t& other) const {
        return timestamp_t(get() + other.get());
    }
    constexpr timestamp_t operator/(const timestamp_t& other) const {
        return timestamp_t(get() / other.get());
    }
    constexpr timestamp_t operator*(const timestamp_t& other) const {
        return timestamp_t(get() * other.get());
    }
    
    std::string toStr() const { return valid() ? std::to_string(get()) : "invalid_time"; }
    static std::string class_name() { return "timestamp"; }
    static timestamp_t fromStr(const std::string& str) { return timestamp_t(std::atoll(str.c_str())); }
};
}

#undef isnormal
#include <misc/math.h>
#include <misc/EnumClass.h>
#include <misc/stringutils.h>

