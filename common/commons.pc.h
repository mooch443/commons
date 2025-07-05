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
//#include <iostream>
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
#include <list>
#include <shared_mutex>
#include <condition_variable>
#include <variant>
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
#include <optional>
#include <compare>
#include <random>
#include <regex>
#include <any>
#include <stack>
#if __APPLE__
#include <Availability.h>
#endif

#if __has_include(<latch>)
    #if !defined(__APPLE__) || defined(__MAC_11_0)
        #include <latch>
        #define COMMONS_HAS_LATCH
    #endif
#elif __has_include(<experimental/latch>)
    #if !defined(__APPLE__) || defined(__MAC_11_0)
        #include <experimental/latch>
        namespace std {
        using experimental;
        }
        #define COMMONS_HAS_LATCH
    #endif
#endif

#include <misc/date.h>
#ifdef __llvm__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#endif
#include <glaze/glaze.hpp>
//#include <nlohmann/json.hpp>
#ifdef __llvm__
#pragma clang diagnostic pop
#endif

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

#if !defined(HAVE_CONCEPT_IMPLEMENTATION)
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

#ifdef WIN32
#define _USE_MATH_DEFINES
#include <cmath>
#endif

using long_t = int32_t;

#define CMN_ASSERT(cond, msg) assert((cond) && (msg))

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
    constexpr source_location( const source_location& other ) = default;
    constexpr source_location( source_location&& other ) noexcept = default;
    
    constexpr source_location& operator=(const source_location&) noexcept = default;
    constexpr source_location& operator=(source_location&&) noexcept = default;

    static constexpr source_location current(
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
#include <robin_hood.h>

#ifndef CMN_WITH_IMGUI_INSTALLED
    #if __has_include ( <imgui.h> )
        #include <imconfig.h>
        #include <imgui.h>
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


#define UNUSED(X) {(void)X;}
#include <misc/expected.h>
#include <misc/useful_concepts.h>
#include <misc/UnorderedVectorSet.h>

namespace cmn {

template <typename T> struct type_t {};
template <typename T> constexpr inline type_t<T> type_v{};

inline void set_thread_name(const std::string& name) {
#if __APPLE__
    if(int result = pthread_setname_np(name.c_str());
       result != 0)
    {
        switch (result) {
            case EINVAL:
                std::cerr << "Invalid argument (name too long or invalid thread ID).\n";
                break;
            case ESRCH:
                std::cerr << "No such thread found.\n";
                break;
            case EPERM:
                std::cerr << "Operation not permitted (insufficient privileges).\n";
                break;
            case ENOMEM:
                std::cerr << "Out of memory.\n";
                break;
            default:
                std::cerr << "Unknown error.\n";
        }
    }
#elif __linux__
    pthread_setname_np(pthread_self(), name.c_str());
#elif defined(WIN32)
    int convertResult = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.length(), NULL, 0);
    if (convertResult <= 0)
    {
        throw std::invalid_argument("Cannot convert string "+name+" to wide char.");
    }
    else
    {
        ::std::wstring wide;
        wide.resize(convertResult + 10);
        convertResult = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (int)name.length(), wide.data(), (int)wide.size());
        if (convertResult > 0) {
            SetThreadDescription(
                GetCurrentThread(),
                wide.c_str()
            );
        }
    }
#endif
}

#if defined(WIN32)
inline std::string WStringToString(const std::wstring& wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], size_needed, NULL, NULL);
    return str;
}
#endif

inline std::string get_thread_name() {
#if !defined(WIN32) && !defined(__EMSCRIPTEN__)
    char buffer[1024];
    pthread_getname_np(pthread_self(), buffer, sizeof(buffer));
    return std::string(buffer);
#else
    PWSTR data;
    HRESULT hr = GetThreadDescription(GetCurrentThread(), &data);
    if (SUCCEEDED(hr))
    {
        ::std::wstring thread_name(data);
        LocalFree(data);

        return WStringToString(thread_name);
    }
    return "<unnamed>";
#endif
}

inline std::string get_thread_name(auto id) {
#if !defined(WIN32) && !defined(__EMSCRIPTEN__)
    char buffer[1024];
    pthread_getname_np(id, buffer, sizeof(buffer));
    return std::string(buffer);
#else
    PWSTR data;
    HRESULT hr = GetThreadDescription(id, &data);
    if (SUCCEEDED(hr))
    {
        ::std::wstring thread_name(data);
        LocalFree(data);

        return WStringToString(thread_name);
    }
    return "<unnamed>";
#endif
}

inline consteval bool is_debug_mode() {
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

inline consteval std::string_view compile_mode_name() {
#ifndef NDEBUG
    return std::string_view("debug");
#elif defined(NDEBUG) && defined(RELWITHDEBINFO)
    return std::string_view("dbgrelease");
#elif defined(NDEBUG)
    return std::string_view("release");
#else
    return std::string_view("unknown");
#endif
}

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

#define GETTER_CONST(TYPE, VAR) protected: TYPE _##VAR; public: [[nodiscard]] inline TYPE& VAR() const { return _##VAR; } protected:
#define GETTER(TYPE, VAR) public: [[nodiscard]] const TYPE& VAR() const { return _##VAR; } protected: TYPE _##VAR
#define GETTER_I(TYPE, VAR, INIT) public: [[nodiscard]] const TYPE& VAR() const { return _##VAR; } protected: TYPE _##VAR = INIT
#define GETTER_NCONST(TYPE, VAR) public: [[nodiscard]] inline const TYPE& VAR() const { return _##VAR; } [[nodiscard]] inline TYPE& VAR() { return _##VAR; } protected: TYPE _##VAR
#define GETTER_PTR(TYPE, VAR) public: [[nodiscard]] inline TYPE VAR() const { return _##VAR; } [[nodiscard]] inline TYPE & VAR() { return _##VAR; } protected: TYPE _##VAR
#define GETTER_PTR_I(TYPE, VAR, INIT) public: [[nodiscard]] inline TYPE VAR() const { return _##VAR; } [[nodiscard]] inline TYPE & VAR() { return _##VAR; } protected: TYPE _##VAR = INIT
#define GETTER_CONST_PTR(TYPE, VAR) protected: TYPE _##VAR; public: inline const TYPE VAR() const { return _##VAR; } protected:
#define GETTER_NREF(TYPE, VAR) protected: TYPE _##VAR; public: [[nodiscard]] inline const TYPE VAR() const { return _##VAR; } [[nodiscard]] inline TYPE VAR() { return _##VAR; } protected:
#define GETTER_SETTER(TYPE, VAR) public: [[nodiscard]] inline const TYPE& VAR() const { return _##VAR; } inline void set_##VAR(const TYPE& value) { _##VAR = value; } protected: TYPE _##VAR
#define GETTER_SETTER_I(TYPE, VAR, INIT) public: [[nodiscard]] inline const TYPE& VAR() const { return _##VAR; } inline void set_##VAR(const TYPE& value) { _##VAR = value; } protected: TYPE _##VAR = INIT
#define GETTER_SETTER_PTR(TYPE, VAR) protected: TYPE _##VAR; public: [[nodiscard]] inline TYPE VAR() const { return _##VAR; } inline void set_##VAR(TYPE value) { _##VAR = value; } protected:
#define GETTER_SETTER_PTR_I(TYPE, VAR, INIT) protected: TYPE _##VAR = INIT; public: [[nodiscard]] inline TYPE VAR() const { return _##VAR; } inline void set_##VAR(TYPE value) { _##VAR = value; } protected:
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

// max_element for containers that are not maps
template <typename Iterable>
std::optional<typename Iterable::value_type> max_element_impl(const Iterable& container, std::false_type) {
    if (container.empty()) {
        return std::nullopt;
    }
    
    return *std::max_element(container.begin(), container.end());
}

// max_element for maps (based on the value part of the pair)
template <typename Map>
std::optional<typename Map::value_type> max_element_impl(const Map& map, std::true_type) {
    if (map.empty()) {
        return std::nullopt;
    }

    // Find the element with the maximum 'mapped_type' (value)
    auto max_iter = std::max_element(map.begin(), map.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });
    
    return *max_iter;
}

// General max_element function that dispatches based on whether the container is a map
template <typename Iterable>
auto max_element(const Iterable& container) {
    return max_element_impl(container, is_map<Iterable>{});
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

template<typename First, typename ... T>
constexpr bool is_in(First &&first, T && ... t)
{
    return ((first == t) || ...);
}

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

template<class T> struct is_array_derived : public std::false_type {};

template<class T, size_t Size>
struct is_array_derived<std::array<T, Size>> : public std::true_type {};

template<typename T, std::size_t N, typename U>
constexpr std::size_t constexpr_find(const std::array<T, N>& arr, const U& value, std::size_t pos = 0) {
    return pos == N ? N : (arr[pos] == value ? pos : constexpr_find(arr, value, pos + 1));
}

template<typename T, typename Q>
    requires (is_container<T>::value || is_queue<T>::value || is_deque<T>::value) && (!is_instantiation<UnorderedVectorSet, T>::value)
  //      && (not is_array_derived<T>::value)
inline constexpr bool contains(const T& s, const Q& value) {
    return std::find(s.begin(), s.end(), value) != s.end();
}

/*template<typename T, typename Q>
    requires is_array_derived<T>::value
inline constexpr bool contains(const T& s, const Q& value) {
    return constexpr_find(s, value) != s.size();
}*/

template<typename T, typename K>
    requires std::convertible_to<K, T>
inline constexpr bool contains(const ska::bytell_hash_set<T>& s, const K& value) {
    return s.count(value) > 0;
}

template<bool IsFlat, size_t MaxLoadFactor100, typename Key, typename T, typename Hash, typename KeyEqual>
inline bool contains(const robin_hood::detail::Table<IsFlat, MaxLoadFactor100, Key, T, Hash, KeyEqual>& v, auto&& obj) {
    return v.contains(std::forward<decltype(obj)>(obj));
}

template<typename T, typename... Args, template <typename...> class K>
    requires (is_set< std::remove_cvref_t<K<Args...>> >::value
              || is_map< std::remove_cvref_t<K<Args...>> >::value)
inline bool contains(const K<Args...>& v, T&& obj) {
    if constexpr(is_instantiation<UnorderedVectorSet, T>::value) {
        return v.contains(std::forward<T>(obj));
    } else {
#if __cplusplus >= 202002L
        return v.contains(std::forward<T>(obj));
#else
        return v.find(std::forward<T>(obj)) != v.end();
#endif
    }
}

template<class T, typename Comparator>
auto insert_sorted(std::vector<T>& vector, T&& element, Comparator&& cmp) {
    return vector.insert(std::upper_bound(vector.begin(), vector.end(), element, std::forward<Comparator>(cmp)), std::move(element));
}

template<class T>
auto insert_sorted(std::vector<T>& vector, T&& element) {
    return vector.insert(std::upper_bound(vector.begin(), vector.end(), element), std::move(element));
}

template<class T, typename Comparator, class K = T>
auto find_sorted(const std::vector<T>& vector, const K& element, Comparator&& cmp) {
    auto it = std::lower_bound(vector.begin(), vector.end(), element, cmp);
    if (it != vector.end() && !cmp(element, *it)) {
        return it; // Element found
    } else {
        return vector.end(); // Element not found
    }
}

template<class T>
auto insert_sorted(std::vector<T>& vector, const T& element) {
    return vector.insert(std::upper_bound(vector.begin(), vector.end(), element), element);
}
}

namespace cmn {

/**
 * @class TrivialOptional
 * @brief A minimal optional implementation for arithmetic types that uses a reserved invalid value.
 *
 * @details
 * Limitations:
 * - Only supports arithmetic types (integral and floating-point).
 * - Uses a sentinel value (e.g., -1 or infinity) which may conflict with valid data values.
 *
 * Advantages:
 * - Zero overhead: occupies the same memory as the underlying type (no extra tag or allocation).
 * - Trivially copyable and constexpr-friendly.
 * - Fast operations with no heap allocations or additional flags.
 *
 * Use std::optional<T> when:
 * - You need a robust optional without sentinel collision risks.
 * - You require full support for all T values, in-place construction, or monadic utilities.
 */
enum class TrivialIllegalValueType {
    NegativeOne,
    Infinity,
    Lowest
};

template<typename Numeric,
         TrivialIllegalValueType InvalidValueType =
               std::unsigned_integral<Numeric>
                 ? TrivialIllegalValueType::NegativeOne
                 : (std::signed_integral<Numeric> || not std::numeric_limits<Numeric>::has_infinity
                       ? TrivialIllegalValueType::Lowest
                       : TrivialIllegalValueType::Infinity)>
    /// TrivialOptional can only be used with integral or floating-point types.
    requires ((std::integral<Numeric> || std::floating_point<Numeric>) && not std::same_as<Numeric, bool>)
class TrivialOptional {
public:
    static_assert(
        std::unsigned_integral<Numeric>
        || std::signed_integral<Numeric>
        || std::floating_point<Numeric>,
        "TrivialOptional can only be used with integral or floating-point types"
    );
    static_assert(
        InvalidValueType != TrivialIllegalValueType::Infinity
                  || std::numeric_limits<Numeric>::has_infinity,
        "Floating-point TrivialOptional requires IEEE-style infinity"
    );

    static constexpr Numeric InvalidValue =
        InvalidValueType == TrivialIllegalValueType::NegativeOne
            ? static_cast<Numeric>(-1)
            : (InvalidValueType == TrivialIllegalValueType::Infinity
                ? std::numeric_limits<Numeric>::infinity()
                : std::numeric_limits<Numeric>::lowest());
    
    static constexpr TrivialIllegalValueType InvalidType = InvalidValueType;
    
private:
    Numeric value_{InvalidValue};

public:
    using value_type = Numeric;
    
    constexpr TrivialOptional() noexcept = default;
    constexpr TrivialOptional(const TrivialOptional&) noexcept = default;
    constexpr TrivialOptional(TrivialOptional&&) noexcept = default;
    constexpr explicit TrivialOptional(Numeric value) noexcept : value_(value) {
        assert(value != InvalidValue);
    }
    constexpr explicit TrivialOptional(std::nullopt_t) noexcept {}
    
    constexpr TrivialOptional& operator=(const TrivialOptional&) noexcept = default;
    constexpr TrivialOptional& operator=(TrivialOptional&&) noexcept = default;
    constexpr TrivialOptional& operator=(Numeric value) noexcept {
        assert(value != InvalidValue);
        value_ = value;
        return *this;
    }
    // Support brace-list assignment: {} resets, {value} assigns that value
    constexpr TrivialOptional& operator=(std::initializer_list<Numeric> il) noexcept {
        if (il.size() == 0) {
            // empty list clears the optional
            return *this = std::nullopt;
        } else {
            // single-element list assigns that value
            assert(il.size() == 1 && "TrivialOptional initializer-list assignment requires at most one element");
            return *this = *il.begin();
        }
    }
    constexpr TrivialOptional& operator=(std::nullopt_t) noexcept {
        value_ = InvalidValue;
        return *this;
    }

    constexpr bool has_value() const noexcept {
        return value_ != InvalidValue;
    }

    constexpr const Numeric& value() const noexcept {
        assert(has_value());
        return value_;
    }

    constexpr Numeric& value() noexcept {
        assert(has_value());
        return value_;
    }

    constexpr Numeric value_or(Numeric default_value) const noexcept {
        return has_value() ? value_ : default_value;
    }

    constexpr void reset() noexcept {
        value_ = InvalidValue;
    }

    constexpr Numeric& operator*() noexcept {
        return value_;
    }

    constexpr const Numeric& operator*() const noexcept {
        return value_;
    }

    constexpr TrivialOptional& operator+=(Numeric value) noexcept {
        value_ += value;
        return *this;
    }
    constexpr TrivialOptional& operator+=(const TrivialOptional& value) noexcept {
        assert(value.has_value());
        assert(has_value());
        value_ += value;
        return *this;
    }

    constexpr Numeric* operator->() noexcept {
        return &value_;
    }

    constexpr const Numeric* operator->() const noexcept {
        return &value_;
    }

    constexpr explicit operator bool() const noexcept {
        return has_value();
    }

    // Comparison operators
    constexpr friend bool operator==(const TrivialOptional& lhs, const TrivialOptional& rhs) {
        return lhs.value_ == rhs.value_;
    }

    constexpr friend bool operator!=(const TrivialOptional& lhs, const TrivialOptional& rhs) {
        return !(lhs == rhs);
    }
    
    std::string toStr() const {
        return has_value() ? std::to_string(value_) : "nullopt";
    }
    
    static std::string class_name() {
        return "TOptional<"+(std::string)typeid(Numeric).name()+">";
    }
    
    glz::json_t to_json() const {
        return has_value() ? glz::json_t{ value() } : glz::json_t{};
    }
    
    /// we do not have Meta:: available here yet, so we have to go with our manual
    /// implementation for fromStr
    static TrivialOptional fromStr(const std::string& str) {
        if (str == "null")
            return {};
        if constexpr (std::is_integral_v<Numeric>) {
            long long v = std::stoll(str);
            return TrivialOptional(static_cast<Numeric>(v));
        } else if constexpr (std::is_floating_point_v<Numeric>) {
            long double v = std::stold(str);
            return TrivialOptional(static_cast<Numeric>(v));
        } else {
            static_assert(std::is_integral_v<Numeric> || std::is_floating_point_v<Numeric>,
                          "TrivialOptional::fromStr not implemented for this type");
        }
    }
};

struct timestamp_t {
    using value_type = uint64_t;
    TrivialOptional<value_type> value;
    
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
    constexpr operator std::chrono::microseconds() const { return std::chrono::microseconds(valid() ? get() : 0); }
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
    
    std::string to_date_string() const {
        if(valid()) {
            auto ms = std::chrono::microseconds(get());
            auto time = std::chrono::time_point<std::chrono::system_clock>(ms);
            std::time_t t = std::chrono::system_clock::to_time_t(time);
            std::tm tm = *std::localtime(&t);
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }
        return "null";
    }
    std::string toStr() const { return valid() ? std::to_string(get()) : "invalid_time"; }
    static std::string class_name() { return "timestamp"; }
    static timestamp_t fromStr(const std::string& str) { return timestamp_t(std::atoll(str.c_str())); }
    glz::json_t to_json() const { return valid() ? glz::json_t{ get() } : glz::json_t{}; }
    
    static timestamp_t now() {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        return timestamp_t(std::chrono::time_point_cast<std::chrono::microseconds>(now).time_since_epoch().count());
    }
};
}

namespace cmn {
#if !defined(__EMSCRIPTEN__) || true
template<typename T>
using atomic = std::atomic<T>;
#else
template<typename T>
struct atomic {
    T value;
    mutable std::mutex m;

    atomic() = default;
    atomic(T v) : value( v ) {}

    T load() const {
        std::unique_lock guard(m);
        return value;
    }

    void store(T v) {
        std::unique_lock guard(m);
        value = v;
    }

    operator T() const {
        return load();
    }

    void operator=(T v) {
        store(v);
    }

    bool operator==(T v) const {
        return load() == v;
    }

    bool operator!=(T v) const {
        return load() != v;
    }

    T operator++() {
        std::unique_lock g(m);
        ++value;
        return value;
    }

    T operator--() {
        std::unique_lock g(m);
        --value;
        return value;
    }
};
#endif

template <typename T>
class read_once {
private:
    std::optional<T> data;
    std::mutex mtx;

public:
    read_once() : data(std::nullopt) {}

    void set(T value) {
        std::scoped_lock lock(mtx);
        if(data) {
#ifndef NDEBUG
            printf("Cannot replace existing value\n.");
#endif
        } else {
            data = std::move(value);
        }
    }

    T read() {
        std::scoped_lock lock(mtx);
        if (data) {
            T value = std::move(*data);
            data.reset();
            return value;
        }
        return T{};
    }
};

struct MultiStringHash {
    using is_transparent = void;  // Enable heterogeneous lookup
    
    size_t operator()(const std::string& str) const {
        return std::hash<std::string>{}(str);
    }

    template<typename Str>
        requires (std::convertible_to<std::string_view, Str> && not std::same_as<std::string, Str>)
    size_t operator()(Str strv) const {
        if constexpr(std::same_as<std::string_view, Str>)
            return std::hash<std::string_view>{}(strv);
        else
            return std::hash<std::string_view>{}(std::string_view(strv));
    }
};

struct MultiStringEqual {
    using is_transparent = void;  // Enable heterogeneous lookup
    
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return lhs == rhs;
    }

    template<typename Str>
        requires (std::convertible_to<std::string_view, Str> && not std::same_as<std::string, Str>)
    bool operator()(const std::string& lhs, Str rhs) const {
        return lhs == rhs;
    }

    template<typename Str>
        requires (std::convertible_to<std::string_view, Str> && not std::same_as<std::string, Str>)
    bool operator()(Str lhs, const std::string& rhs) const {
        return lhs == rhs;
    }

    template<typename Str, typename Str2>
        requires (std::convertible_to<std::string_view, Str> && not std::same_as<std::string, Str>
                  && std::convertible_to<std::string_view, Str2> && not std::same_as<std::string, Str2>)
    bool operator()(Str lhs, Str2 rhs) const {
        return lhs == rhs;
    }
};

struct CallbackCollection {
    std::unordered_map<std::string, std::size_t, MultiStringHash, MultiStringEqual> _ids;
    operator bool() const { return not _ids.empty(); }
    void reset() { _ids.clear(); }
};

/// @brief Template for compile-time type name extraction.
template <typename T> constexpr std::string_view type_name();

/// @brief Specialization of type_name for the 'void' type.
/// @return "void" as a compile-time string.
template <>
constexpr std::string_view type_name<void>()
{ return "void"; }

namespace detail {

/// @brief Dummy type used to probe the type-name layout from the compiler.
using type_name_prober = void;

/// @brief Internal utility to get the full signature of a function or callable type.
/// The exact output depends on the compiler being used.
template <typename T>
constexpr std::string_view wrapped_type_name()
{
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif
}

/// @brief Computes the length of the prefix that precedes the actual type name.
/// For example, in GCC, "__PRETTY_FUNCTION__" might produce
/// "constexpr std::string_view detail::wrapped_type_name() [T = int]"
/// and the prefix is "constexpr std::string_view detail::wrapped_type_name() [T = ".
constexpr std::size_t wrapped_type_name_prefix_length() {
    return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

/// @brief Computes the length of the suffix that follows the actual type name.
/// For example, in GCC, "__PRETTY_FUNCTION__" might produce
/// "constexpr std::string_view detail::wrapped_type_name() [T = int]"
/// and the suffix is "]".
constexpr std::size_t wrapped_type_name_suffix_length() {
    return wrapped_type_name<type_name_prober>().length()
    - wrapped_type_name_prefix_length()
    - type_name<type_name_prober>().length();
}

} // namespace detail

/// @brief Main function to extract the type name.
/// This function removes the prefix and suffix from the compiler-specific full signature
/// to isolate and return the actual type name.
template <typename T>
constexpr std::string_view type_name() {
    constexpr auto wrapped_name = detail::wrapped_type_name<T>();
    constexpr auto prefix_length = detail::wrapped_type_name_prefix_length();
    constexpr auto suffix_length = detail::wrapped_type_name_suffix_length();
    constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
    return wrapped_name.substr(prefix_length, type_name_length);
}

//#define CMN_DEBUG_MUTEXES
#if defined(CMN_DEBUG_MUTEXES)

template<typename Mutex>
class LoggedLock;

template<typename Mutex = std::mutex>
class LoggedMutex {
    Mutex* _mtx;
    std::string _thread_name;
    const char *_name;
public:
    const char* name() const { return _name; }
    
    LoggedMutex(const char* name, source_location c = source_location::current()) : _mtx(new Mutex()), _name(name) {
        printf("Constructing Mutex %s at address: %llX @ %s::%s:%u\n", _name, (uint64_t)_mtx, c.file_name(), c.function_name(), c.line());
    }
    ~LoggedMutex() {
        printf("Destroying Mutex %s at address: %llX\n",_name,(uint64_t)_mtx);
    }
    
private:
    friend class LoggedLock<LoggedMutex<Mutex>>;
    void lock(source_location c = source_location::current()) {
        printf("Locking Mutex %s at address: %llX @ %s::%s:%u\n",_name,(uint64_t)_mtx, c.file_name(), c.function_name(), c.line());
        _mtx->lock();
        _thread_name = get_thread_name();
    }

    bool try_lock() {
        printf("Attempt locking Mutex %s at address: %lX\n",_name,(uint64_t)_mtx);
        if(_mtx->try_lock()) {
            _thread_name = get_thread_name();
            return true;
        } else
            return false;
    }

    void unlock(source_location c = source_location::current()) {
        printf("Unlocking Mutex %s at address: %llX @ %s::%s:%u\n",_name,(uint64_t)_mtx,c.file_name(), c.function_name(), c.line());
        _mtx->unlock();
    }
    
};

template<typename T>
concept mutex_callable_with_loc = requires(T t) {
    { t.lock(source_location::current()) } -> std::same_as<void>;
};

// Specialization for your custom Mutex
template<typename MutexType>
class LoggedLock {
    source_location _c;
    MutexType* _mutex{nullptr};
    bool _owns_lock{false};
    const char * _name = "<none>";

public:
    LoggedLock(MutexType& m, source_location c)
        : _c(c), _mutex(&m), _owns_lock(false) {
            if constexpr(mutex_callable_with_loc<MutexType>) {
                _name = m.name();
            }
        lock();
    }
    
    LoggedLock(source_location c) noexcept
        : _c(c)
    {
        
    }

    ~LoggedLock() {
        if (_owns_lock && _mutex) {
            _mutex->unlock();
        }
    }

    void lock() {
        if(not _mutex)
            throw std::invalid_argument("mutex is nullptr");
        if (!_owns_lock) {
            if constexpr(mutex_callable_with_loc<MutexType>) {
                printf("Attempting to lock _mutex at %s:%u (%s)\n", _c.file_name(), _c.line(), _name);
                _mutex->lock(_c);
            } else {
                printf("Attempting to lock _mutex at %s:%u\n", _c.file_name(), _c.line());
                _mutex->lock();
            }
            _owns_lock = true;
        }
    }

    void unlock() {
        if(not _mutex)
            throw std::invalid_argument("mutex is nullptr");
        if (_owns_lock) {
            if constexpr(mutex_callable_with_loc<MutexType>) {
                printf("Attempting to unlock _mutex at %s:%u (%s)\n", _c.file_name(), _c.line(), _name);
                _mutex->unlock(_c);
            } else {
                printf("Attempting to unlock _mutex at %s:%u\n", _c.file_name(), _c.line());
                _mutex->unlock();
            }
            
            _owns_lock = false;
        }
    }
    
    void swap(LoggedLock& other) noexcept {
        using std::swap;
        swap(_mutex, other._mutex);
        swap(_owns_lock, other._owns_lock);
        swap(_name, other._name);
        swap(_c, other._c);
    }

    bool owns_lock() const noexcept {
        return _owns_lock;
    }
};

#define LOGGED_LOCK_TYPE LoggedLock
#define LOGGED_LOCK(MUTEX) LOGGED_LOCK_TYPE (MUTEX, cmn::source_location::current())
#define LOGGED_LOCK_VAR_TYPE(TYPE) LOGGED_LOCK_TYPE < LoggedMutex< TYPE > > (cmn::source_location::current())
#define LOGGED_MUTEX_TYPE LoggedMutex<>
#define LOGGED_MUTEX(NAME) LOGGED_MUTEX_TYPE (NAME, cmn::source_location::current())
#define LOGGED_MUTEX_VAR(VAR, NAME) LOGGED_MUTEX_TYPE VAR{NAME, cmn::source_location::current()}
#define LOGGED_MUTEX_VAR_TYPE(TYPE, VAR, NAME) LoggedMutex<TYPE> VAR{NAME, cmn::source_location::current()}

#else

#define LOGGED_LOCK_TYPE std::unique_lock
#define LOGGED_LOCK(MUTEX) LOGGED_LOCK_TYPE { MUTEX }
#define LOGGED_LOCK_VAR_TYPE(TYPE) LOGGED_LOCK_TYPE < TYPE > { }
#define LOGGED_LOCK_VAR(TYPE, MUTEX) LOGGED_LOCK_TYPE < TYPE > { MUTEX }
#define LOGGED_MUTEX_TYPE std::mutex
#define LOGGED_MUTEX(NAME) LOGGED_MUTEX_TYPE { }
#define LOGGED_MUTEX_VAR(VAR, NAME) LOGGED_MUTEX_TYPE VAR{}
#define LOGGED_MUTEX_VAR_TYPE(TYPE, VAR, NAME) TYPE VAR{}

#endif

}

//#undef isnormal
#include <misc/math.h>
#include <misc/stringutils.h>
#include <types.h>
