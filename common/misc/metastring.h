#pragma once

#include <commons.pc.h>
#include <version>

#if __has_include(<charconv>)
#include <charconv>
#endif

#ifdef __APPLE__
#   include <Availability.h>
#   if __ENVIRONMENT_OS_VERSION_MIN_REQUIRED__ >= __MAC_13_3
#       define trex_use_std_to_chars
#   endif
#elif __cplusplus >= 201703L && (defined(__cpp_lib_constexpr_charconv) || defined(__cpp_lib_to_chars))
#   define trex_use_std_to_chars
#endif

namespace cmn {

#if defined(trex_use_std_to_chars)
// C++17 and std::to_chars is available
using std::to_chars;
using std::to_chars_result;
using std::chars_format;
#else
struct to_chars_result
{
    char* ptr;
    std::errc ec;
};

enum class chars_format
{
    scientific = 0x1,
    fixed = 0x2,
    hex = 0x4,
    general = fixed | scientific
};

// Define a custom implementation of to_chars
template <typename T>
to_chars_result to_chars(char* first, const char* last, T value) {
    auto str = std::to_string(value);
    if (str.size() > static_cast<size_t>(last - first)) {
        return {first, std::errc::value_too_large};
    }
    std::copy(str.begin(), str.end(), first);
    return {first + str.size(), std::errc{}};
}

template <typename T>
to_chars_result to_chars(char* first, const char* last, T value, chars_format format) {
    if(format == chars_format::general) {
        return to_chars(first, last, value);
    }
    
    std::stringstream ss;
    if(format == chars_format::hex)
        ss << std::hex;
    else if(format == chars_format::fixed)
        ss << std::fixed;
    else if(format == chars_format::scientific)
        ss << std::scientific;
    
    ss << value;
    auto str = ss.str();
    std::copy(str.begin(), str.end(), first);
    return {first + str.size(), std::errc{}};
}
#endif
    
class illegal_syntax : public std::logic_error {
public:
    illegal_syntax(const std::string& str) : std::logic_error(str) { }
    ~illegal_syntax() throw() { }
};
    
struct DurationUS {
    //! A duration in microseconds.
    timestamp_t timestamp;
        
    std::string to_string() const {
        static constexpr std::array<std::string_view, 5> names{{"us", "ms", "s", "min", "h"}};
        static constexpr std::array<double, 5> ratios{{1000, 1000, 60, 60, 24}};
            
        double scaled = static_cast<double>(timestamp.get()), previous_scaled = 0;
        size_t i = 0;
        while(i < ratios.size()-1 && scaled >= ratios[i]) {
            scaled /= ratios[i];
                
            previous_scaled = scaled - size_t(scaled);
            previous_scaled *= ratios[i];
                
            i++;
        }
            
        size_t sub_part = (size_t)previous_scaled;
            
        std::stringstream ss;
        ss << std::fixed << std::setprecision(0) << scaled;
        if(i>0 && i > 2)
            ss << ":" << std::setfill('0') << std::setw(2) << sub_part;
        ss << std::string(names[i].begin(), names[i].end());
        return ss.str();
    }
        
    std::string to_html() const {
        static constexpr std::array<std::string_view, 5> names{{"us", "ms", "s", "min", "h"}};
        static constexpr std::array<double, 5> ratios{{1000, 1000, 60, 60, 24}};
            
        double scaled = static_cast<double>(timestamp.get()), previous_scaled = 0;
        size_t i = 0;
        while(i < ratios.size()-1 && scaled >= ratios[i]) {
            scaled /= ratios[i];
                
            previous_scaled = scaled - size_t(scaled);
            previous_scaled *= ratios[i];
                
            i++;
        }
            
        size_t sub_part = (size_t)previous_scaled;
            
        std::stringstream ss;
        ss << "<nr>" << std::fixed << std::setprecision(0) << scaled << "</nr>";
        if(i>0 && i > 2)
            ss << ":<nr>" << std::setfill('0') << std::setw(2) << sub_part << "</nr>";
        ss << std::string(names[i].begin(), names[i].end());
        return ss.str();
    }
    
    static std::string class_name() { return "duration"; }
    std::string toStr() const {
        return to_string();
    }
};
    
struct FileSize {
    uint64_t bytes;
        
    FileSize(uint64_t b = 0) : bytes(b) {}
        
    std::string to_string() const {
        std::vector<std::string> descriptions = {
            "bytes", "KB", "MB", "GB", "TB"
        };
            
        size_t i=0;
        auto scaled = static_cast<double>(bytes);
        while (scaled >= 1000 && i < descriptions.size()-1) {
            scaled /= 1000.0;
            i++;
        }
            
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << scaled << descriptions[i];
            
        return ss.str();
    }

    static std::string class_name() {
        return "filesize";
    }
    std::string toStr() const {
        return to_string();
    }
};

// <util>
#pragma region util
namespace util {

const char* to_readable_errc(const std::errc& error_code);

template<size_t N>
class ConstexprString {
private:
    std::array<char, N> _data{ 0 };

public:
    constexpr size_t capacity() const noexcept { return N - 1u; }
    
    // Constructor from const char array
    template<size_t M>
        requires (M < N)
    constexpr ConstexprString(const char (&arr)[M]) noexcept {
        for (size_t i = 0; i < M; ++i) {
            _data[i] = arr[i];
        }
        _data[M] = 0;
    }

    // Constructor from std::array
    template<size_t M>
        requires (M<N)
    constexpr ConstexprString(const std::array<char, M>& arr) noexcept
    {
        for(size_t i=0; i<M; ++i)
            _data[i] = arr[i];
        _data[M] = 0;
    }

    constexpr char* data() { return _data.data(); }
    constexpr const char* data() const { return _data.data(); }

    // Default constructor
    constexpr ConstexprString() = default;

    // Accessor to get std::string_view
    constexpr std::string_view view() const {
        return std::string_view(_data.data());
    }

    // Other utility methods as needed, e.g., size, operator[], etc.
    constexpr size_t size() const {
        return view().length();
    }

    constexpr char operator[](size_t index) const {
        return view()[index];
    }

    constexpr char& operator[](size_t index) {
        return *(data() + index);
    }

    template<size_t L>
    constexpr bool operator==(const ConstexprString<L>& other) const noexcept {
        return other.view() == view();
    }

    constexpr bool operator==(const std::string_view& other) const noexcept {
        return other == view();
    }

    // Constructor from const char array
    template<size_t M>
    constexpr bool operator==(const char (&arr)[M]) const noexcept {
        return std::string_view(arr, M-1) == view();
    }

    constexpr explicit operator std::string() const noexcept {
        return std::string(view());
    }
};

using ConstString_t = ConstexprString<128>;

template <typename T>
requires std::floating_point<T>
constexpr auto to_string(T value) {
    ConstString_t contents; // Initialize array with null terminators

    if (std::is_constant_evaluated())
    {
        if (value != value) {
            const char nan[] = "nan";
            std::copy(std::begin(nan), std::end(nan) - 1, contents.data()); // -1 to exclude null terminator
            return contents;
        }
        if (value == std::numeric_limits<T>::infinity() || value == -std::numeric_limits<T>::infinity()) {
            const char* inf = (value < 0) ? "-inf" : "inf";
            std::copy(inf, inf + std::string_view(inf).length(), contents.data());
            return contents;
        }

        constexpr int max_digits = 50;
        // Extract integer and fractional parts
        int64_t integerPart = static_cast<int64_t>(value);
        T fractionalPart = value - integerPart;
        if(fractionalPart < 0)
            fractionalPart = -fractionalPart;

        // Convert integer part directly into 'contents'
        char* current = contents.data() + max_digits + 1; // Make room for the largest integer
        if (integerPart < 0) {
            integerPart = -integerPart;  // Convert to positive
            do {
                --current;
                *current = '0' + integerPart % 10; // This should now be '+', not '-'
                integerPart /= 10;
            } while (integerPart != 0);
            
            --current;
            *current = '-';
        } else {
            do {
                --current;
                *current = '0' + integerPart % 10;
                integerPart /= 10;
            } while (integerPart != 0);
        }
        
        // Adjust contents starting point
        auto n = max_digits + 2 - (current - contents.data());
        std::move(current, current + n, contents.data());
        for(size_t i = n; i < contents.size(); ++i) {
            contents[i] = 0;
        }
        //std::fill(contents.data() + n, contents.data() + contents.size(), 0);
        
        // Convert fractional part
        std::string_view tmp(contents.data());
        char* fraction_start = contents.data() + tmp.size();
        *fraction_start = '.';
        char* fraction_current = fraction_start + 1;

        constexpr T tolerance{1e-4};
        char* last_non_zero = 0;
        for (int i = 0; i < max_digits; ++i) {
            fractionalPart *= 10;
            char digit = '0' + static_cast<int>(fractionalPart);
            *fraction_current = digit;
            if (digit != '0') {
                last_non_zero = fraction_current;
            }
            fraction_current++;
            fractionalPart -= static_cast<int>(fractionalPart);
            
            if (fractionalPart < tolerance && fractionalPart > -tolerance) {
                break;  // Break when the fractional part is close enough to zero
            }
        }

        if (last_non_zero) {
            *(last_non_zero + 1) = '\0'; // Trim the trailing zeros
        } else {
            *fraction_start = '\0'; // If the fractional part is all zeros, remove it.
        }
        
        return contents;
    } else {
        if (std::isnan(value)) {
            const char nan[] = "nan";
            std::copy(std::begin(nan), std::end(nan) - 1, contents.data()); // -1 to exclude null terminator
            return contents;
        }
        if (std::isinf(value)) {
            const char* inf = (value < 0) ? "-inf" : "inf";
            std::copy(inf, inf + std::string_view(inf).length(), contents.data());
            return contents;
        }

        // Fallback to cmn::to_chars for runtime
        auto result = cmn::to_chars(contents.data(), contents.data() + contents.capacity(), value, cmn::chars_format::fixed);
        if (result.ec == std::errc{}) {
            // Trim trailing zeros but leave at least one digit after the decimal point
            /*if (not utils::contains((const char*)contents.data(), '.')) {
                *result.ptr = '\0';
                return contents;
            }*/
            char* end = result.ptr;
            while (end > contents.data() + 1 && *(end - 1) == '0')
                --end;
            if(end > contents.data() && *(end - 1) == '.') {
                /// found the decimal point, zero here and we're done.
                *(end - 1) = '\0';
                
            } else if(end > contents.data()) {
                /// if we couldn't find a decimal point so far, we may or may
                /// not have a decimal point in here at all. if we don't then we
                /// are dealing with an integer essentially. in that case
                /// we shouldn't cut off _any_ numbers at all.
                auto ptr = end;
                while(ptr > contents.data() && *ptr != '.')
                    --ptr;
                
                if(ptr > contents.data() && *ptr == '.') // found decimal pt.
                {
                    *end = '\0';
                } else {
                    *result.ptr = '\0';
                }
                
            } else {
                *result.ptr = '\0';
            }
            return contents;
        } else {
            throw std::invalid_argument("Error converting floating point to string");
        }
    }
}

template<typename T>
requires std::integral<T>
constexpr std::string to_string(T value) {
    if (std::is_constant_evaluated()) {
        // For simplicity, we'll assume a maximum of 20 chars for int64_t (and its negative sign).
        std::array<char, 21> buffer;
        char* end = buffer.data() + 20;
        *end = '\0';
        char* current = end;
        
        if (value < 0) {
            do {
                --current;
                *current = '0' - value % 10;
                value /= 10;
            } while (value != 0);
            
            --current;
            *current = '-';
        } else {
            do {
                --current;
                *current = '0' + value % 10;
                value /= 10;
            } while (value != 0);
        }
        
        return std::string(current, end - current);
    } else {
        char buffer[128];
        auto result = cmn::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec == std::errc{}) {
            return std::string{buffer, result.ptr};
        } else {
            throw std::invalid_argument("Error converting to string");
        }
    }
}

template <typename T>
    requires std::convertible_to<T, std::string>
        && (!std::convertible_to<T, std::string>)
        && (!std::is_constructible_v<std::string, T>)
        && (!_has_tostr_method<T>)
std::string to_string(const T& t) {
    return "\""+(std::string)t+"\"";
}
template <typename T>
    requires (!std::convertible_to<T, std::string>)
             && (!std::floating_point<T>)
            && (!std::integral<T>)
std::string to_string(const T& t) {
    return std::to_string (t);
}

template<typename Str>
auto truncate(const Str& str) {
    using StrDecayed = std::remove_cvref_t<Str>;
    std::string_view sv(str);

    if ((utils::beginsWith(sv, '{') && utils::endsWith(sv, '}'))
        || (utils::beginsWith(sv, '[') && utils::endsWith(sv, ']'))) {

        std::string_view trimmed = utils::trim(sv.substr(1, sv.size() - 2));

        if constexpr (std::is_rvalue_reference_v<Str&&>
                      || (std::is_lvalue_reference_v<Str> && std::is_const_v<std::remove_reference_t<Str>>)) 
        {
            if constexpr (std::is_same_v<StrDecayed, std::string>) {
                return std::string(trimmed);
            } else
                return trimmed;
        } else
            return trimmed;
    }

    throw illegal_syntax("Cannot parse array " + std::string(sv) + ".");
}

inline std::string escape(std::string str) {
    str = utils::find_replace(str, "\\", "\\\\");
    return utils::find_replace(str, "\"", "\\\"");
}

inline std::string unescape(std::string str) {
    str = utils::find_replace(str, "\\\"", "\"");
    return utils::find_replace(str, "\\\\", "\\");
}

template<typename Str>
std::vector<std::string> parse_array_parts(const Str& str, const char delimiter = ',') {
    std::stack<char> brackets;
    std::string value;
    std::vector<std::string> ret;

    char prev = 0;
    std::string_view sv(str);  // Convert input to string_view for uniform handling
    value.reserve(sv.size());  // Pre-allocate the maximum possible size

    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        bool in_string = !brackets.empty() && (brackets.top() == '\'' || brackets.top() == '"');

        if (in_string) {
            if (prev != '\\' && c == brackets.top()) {
                brackets.pop();
            }
            value.push_back(c);
        } else {
            switch (c) {
                case '[':
                case '{':
                case '"':
                case '\'':
                    brackets.push(c);
                    break;
                case ']':
                    if (!brackets.empty() && brackets.top() == '[')
                        brackets.pop();
                    break;
                case '}':
                    if (!brackets.empty() && brackets.top() == '{')
                        brackets.pop();
                    break;
                default:
                    break;
            }

            if (brackets.empty()) {
                if (c == delimiter) {
                    ret.emplace_back(utils::trim(value));
                    value.clear();
                } else {
                    value.push_back(c);
                }
            } else {
                value.push_back(c);
            }
        }

        if (prev == '\\' && c == '\\') {
            prev = 0;
        } else {
            prev = c;
        }
    }

    if (!value.empty() || prev == delimiter) {
        ret.emplace_back(utils::trim(value));
    }

    return ret;
}

}
#pragma endregion util
// </util>

// <Meta prototypes>
#pragma region Meta prototypes
namespace Meta {


template<typename Q>
    requires std::is_base_of_v<std::exception, Q>
inline std::string name() {
    return "exception";
}

template<>
inline std::string name<std::invalid_argument>() {
    return "invalid_argument";
}

template<> 
inline std::string name<illegal_syntax>() {
    return "illegal_syntax";
}

template<typename Q>
inline std::string name(const typename std::enable_if< std::is_pointer<Q>::value && !std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q>
inline std::string toStr(Q value, const typename std::enable_if< std::is_pointer<Q>::value && !std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q, typename K = typename cmn::remove_cvref<Q>::type>
    requires is_instantiation<std::atomic, K>::value
inline std::string toStr(const Q& value);

template<typename Q>
inline std::string name(const typename std::enable_if< std::is_pointer<Q>::value && std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q>
inline std::string toStr(Q value, const typename std::enable_if< std::is_pointer<Q>::value && std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q, typename T = typename cmn::remove_cvref<Q>::type>
inline T fromStr(const std::string& str, const typename std::enable_if< std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
inline std::string toStr(const Q& value, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q, typename T = typename cmn::remove_cvref<Q>::type>
inline T fromStr(const std::string& str, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
}
#pragma endregion Meta prototypes
// </Meta prototypes>

// <_Meta>
#pragma region _Meta
namespace _Meta {
        
/**
    * These methods return the appropriate type name as a string.
    * Such as "vector<pair<int,float>>".
    */
//template<class Q> std::string name(const typename std::enable_if< std::is_integral<typename std::remove_cv<Q>::type>::value && !std::is_same<bool, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return sizeof(Q) == sizeof(long) ? "long" : "int"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<int, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "int"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<short, typename std::remove_cv<Q>::type>::value && !std::is_same<int16_t, short>::value, Q >::type* =nullptr) { return "short"; }
template<class Q> std::string name(const typename std::enable_if< !std::is_same<int32_t, int>::value && std::is_same<int32_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "int32"; }
template<class Q> std::string name(const typename std::enable_if< !std::is_same<uint32_t, unsigned int>::value && std::is_same<uint32_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "uint32"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<int16_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "int16"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<uint16_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "uint16"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<unsigned int, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "uint"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<unsigned short, typename std::remove_cv<Q>::type>::value && !std::is_same<uint16_t, unsigned short>::value, Q >::type* =nullptr) { return "ushort"; }
template<class Q> std::string name(const typename std::enable_if< !std::is_same<uint64_t, unsigned long>::value && std::is_same<uint64_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "uint64"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<unsigned long, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "ulong"; }
template<class Q> std::string name(const typename std::enable_if< !std::is_same<int64_t, long>::value && std::is_same<int64_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "int64"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<long, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "long"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<uint8_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "uchar"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<int8_t, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "char"; }
    
template<class Q> std::string name(const typename std::enable_if< std::is_floating_point<typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return sizeof(double) == sizeof(Q) ? "double" : "float"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<std::string, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "string"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<std::wstring, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "wstring"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<bool, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "bool"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<cv::Mat, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "mat"; }
template<class Q> std::string name(const typename std::enable_if< std::is_same<cv::Range, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "range"; }

template<typename Q>
    requires (_is_dumb_pointer<Q> && not std::same_as<Q, const char*>)
std::string name();
        
/**
 * chrono:: time objects
 */
template<class Q>
    requires _has_class_name<Q>
std::string name() {
    return Q::class_name();
}
    
template< template < typename...> class Tuple, typename ...Ts >
std::string tuple_name (Tuple< Ts... >&& tuple)
{        
    std::stringstream ss;
    ss << "tuple<";
    std::apply([&](auto&&... args) {
        ((ss << Meta::name<decltype(args)>() << ","), ...);
    }, std::forward<Tuple<Ts...>>(tuple));
        
    ss << ">";
        
    return ss.str();
}
    
template<class Q>
    requires (is_instantiation<std::tuple, Q>::value)
std::string name() { 
    return tuple_name(Q{});
}
    
template<class Q>
std::string name(const typename std::enable_if< is_container<Q>::value, Q >::type* =nullptr) {
    return "array<"+Meta::name<typename Q::value_type>()+">";
}
template<class Q>
std::string name(const typename std::enable_if< is_queue<Q>::value, Q >::type* =nullptr) {
    return "queue<"+Meta::name<typename Q::value_type>()+">";
}
template<class Q>
std::string name(const typename std::enable_if< is_set<Q>::value, Q >::type* =nullptr) {
    return "set<"+Meta::name<typename Q::value_type>()+">";
}
template<class Q>
std::string name(const typename std::enable_if< is_map<Q>::value, Q >::type* =nullptr) {
    return "map<"+Meta::name<typename Q::key_type>()+","+name<typename Q::mapped_type>()+">";
}
template<class Q>
std::string name(const typename std::enable_if< is_pair<Q>::value, Q >::type* =nullptr) {
    return "pair<"+Meta::name<typename Q::first_type>()
            +","+Meta::name<typename Q::second_type>()+">";
}
template<class Q>
    requires (is_instantiation<cv::Size_, Q>::value)
std::string name() {
    return "size<"+Meta::name<typename Q::value_type>()+">";
}
template<class Q>
    requires (is_instantiation<cv::Rect_, Q>::value)
std::string name() {
    return "rect<"+Meta::name<typename Q::value_type>()+">";
}

template<typename Q>
    requires (_is_dumb_pointer<Q> && not std::same_as<Q, const char*>)
std::string name() {
    return Meta::name<typename std::remove_pointer<typename cmn::remove_cvref<Q>::type>::type>() + "*";
}
        
/**
    * The following methods convert any given value to a string.
    * All objects should be parseable by JavaScript as JSON.
    */
        
template<class Q>
    requires (not is_map<Q>::value && (is_container<Q>::value || is_set<Q>::value || is_deque<Q>::value))
std::string toStr(const Q& obj) {
    std::stringstream ss;
    auto start = obj.begin(), end = obj.end();
    for(auto it=start; it != end; ++it) {
        if(it != start)
            ss << ",";
        ss << Meta::toStr(*it);
    }
    return "[" + ss.str() + "]";
}
    
template<class Q>
    requires (is_queue<Q>::value) && (!is_deque<Q>::value)
std::string toStr(const Q& obj) {
    return "queue<size:"+Meta::toStr(obj.size())+">";
}
        
template<class Q>
    requires (std::convertible_to<Q, std::string> || (std::is_constructible_v<std::string, Q>))
        && (!(is_instantiation<std::tuple, Q>::value))
        && (!_has_tostr_method<Q>)
        && (!_is_number<Q>)
std::string toStr(const Q& obj) {
    return "\"" + util::escape(std::string(obj)) + "\"";
}
        
template<class Q>
    requires _clean_same<cv::Range, Q>
std::string toStr(const Q& obj) {
    return "[" + Meta::toStr(obj.start) + "," + Meta::toStr(obj.end) + "]";
}
        
/*template<class Q, class type>
std::string toStr(const cmn::Range<type>& obj) {
    return "[" + Meta::toStr(obj.start) + "," + Meta::toStr(obj.end) + "]";
}*/

template<class Q>
    requires _clean_same<bool, Q>
std::string toStr(Q obj) {
    return obj == true ? "true" : "false";
}
        
template<class TupType, size_t... I>
std::string tuple_str(const TupType& _tup, std::index_sequence<I...>)
{
            
    std::stringstream str;
    str << "[";
    (..., (str << (I == 0? "" : ",") << Meta::toStr(std::get<I>(_tup))));
    str << "]";
    return str.str();
}
        
template<class... Q>
std::string tuple_str (const std::tuple<Q...>& _tup)
{
    return tuple_str(_tup, std::make_index_sequence<sizeof...(Q)>());
}
        
template<class Q>
    requires (is_instantiation<std::tuple, Q>::value)
std::string toStr(const Q& obj) {
    return tuple_str(obj);
}
        
template<class Q>
    requires _is_number<Q>
std::string toStr(const Q& obj) {
    return (std::string)util::to_string(obj);
}
        
template<class Q>
    requires _clean_same<std::wstring, Q>
std::string toStr(const Q& obj) {
    return ws2s(obj);
}
    
template<class Q>
    requires (is_pair<Q>::value)
std::string toStr(const Q& obj) {
    return "[" + Meta::toStr(obj.first) + "," + Meta::toStr(obj.second) + "]";
}
        
template<class Q>
    requires (is_map<Q>::value)
std::string toStr(const Q& obj) {
    std::stringstream ss;
    ss << "{";
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if(it != obj.begin())
            ss << ',';
        ss << Meta::toStr(it->first);
        ss << ':';
        ss << Meta::toStr(it->second);
                
    }
    ss << "}";
            
    return ss.str();
}
        
template<class Q>
    requires (is_instantiation<cv::Rect_, Q>::value)
std::string toStr(const Q& obj) {
    return "[" + Meta::toStr(obj.x) + "," + Meta::toStr(obj.y) + "," + Meta::toStr(obj.width) + "," + Meta::toStr(obj.height) + "]";
}
        
template<class Q>
    requires _is_smart_pointer<Q> && _has_class_name<typename Q::element_type>
std::string toStr(const Q& obj) {
    using K = typename Q::element_type;
    return "ptr<"+K::class_name() + ">" + (obj == nullptr ? "null" : Meta::toStr<K>(*obj));
}

template<class Q, 
    class C = typename std::remove_reference<Q>::type, 
    class K = typename std::remove_pointer<C>::type>
  requires _is_dumb_pointer<C> && _has_class_name<K> && _has_tostr_method<K>
std::string toStr(C obj) {
    return "ptr<"+K::class_name()+">" + obj->toStr();
}
        
template<class Q>
    requires _has_tostr_method<Q>
            //&& (!std::convertible_to<Q, std::string>)
            //&& (!std::is_constructible_v<std::string, Q>)
            && (!_is_number<Q>)
std::string toStr(const Q& obj) {
    return obj.toStr();
}
    
template<class Q>
    requires _is_smart_pointer<Q> && (!_has_tostr_method<typename Q::element_type>)
std::string toStr(const Q& obj) {
    return "ptr<?>0x" + Meta::toStr(uint64_t(obj.get()));//MetaType<typename std::remove_pointer<typename Q::element_type>::type>::toStr(*obj);
}
        
template<class Q>
    requires (is_instantiation<cv::Size_, Q>::value)
std::string toStr(const Q& obj) {
    return "[" + Meta::toStr(obj.width) + "," + Meta::toStr(obj.height) + "]";
}
        
template<class Q>
    requires _clean_same<cv::Mat, Q>
std::string toStr(const Q& obj)
{
    auto mat = *((const cv::Mat*)&obj);
    std::stringstream ss;
            
    ss << "[";
    for (int i=0; i<mat.rows; i++) {
        if(mat.rows > 15 && (i > 5 && i < mat.rows-6)) {
            //if(i < 7 || i > mat.rows-8)
            //    ss << ",";
            //continue;
        }
                
        ss << "[";
        for (int j=0; j<mat.cols; j++) {
            if(mat.cols > 15 && (j > 5 && j < mat.cols-6)) {
                //if(j < 8 || j > mat.cols-9)
                //    ss << ",";
                //continue;
            }
                    
            switch(mat.type()) {
                case CV_8UC1:
                    ss << mat.at<uchar>(i, j);
                    break;
                case CV_32FC1:
                    ss << mat.at<float>(i, j);
                    break;
                case CV_64FC1:
                    ss << mat.at<double>(i, j);
                    break;
                default:
                    throw std::invalid_argument("unknown matrix type");
            }
            if(j < mat.cols-1)
                ss << ",";
        }
        ss << "]";
        if (i < mat.rows-1) {
            ss << ",";
        }
    }
            
    ss << "]";
    return ss.str();
}
        
/**
    * The following methods convert a string like "5.0" to the native
    * datatype (in this case float or double).
    * Values in the string are expected to represent datatype T.
    * Invalid values will throw exceptions.
    */
template<class Q>
    requires std::signed_integral<typename std::remove_cv<Q>::type> 
             && (!_clean_same<bool, Q>)
Q fromStr(const std::string& str)
{
    if(!str.empty() && str[0] == '\'' && str.back() == '\'')
        return Q(std::stoll(str.substr(1,str.length()-2)));
    return Q(std::stoll(str));
}
template<class Q>
    requires std::unsigned_integral<typename std::remove_cv<Q>::type> 
             && (!_clean_same<bool, Q>)
Q fromStr(const std::string& str)
{
    if (!str.empty() && str[0] == '\'' && str.back() == '\'')
        return Q(std::stoull(str.substr(1, str.length() - 2)));
    return Q(std::stoull(str));
}
    
template<class Q>
    requires _has_fromstr_method<Q>
Q fromStr(const std::string& str) {
    return Q::fromStr(str);
}
        
template<class Q>
    requires std::floating_point<typename std::remove_cv<Q>::type>
Q fromStr(const std::string& str)
{
#if defined(__clang__)
    if(!str.empty() && str[0] == '\'' && str.back() == '\'')
        return static_cast<Q>(std::stod(str.substr(1,str.length()-2)));
    return static_cast<Q>(std::stod(str));
#else
    std::string_view sv(str);
    if(!sv.empty() && sv[0] == '\'' && sv.back() == '\'')
        sv = sv.substr(1,sv.length()-2);

    Q value;
    std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), value, std::chars_format::fixed);
    if (result.ec != std::errc{})
        throw std::runtime_error("Cannot convert value: " + (std::string)sv + " to " + Meta::name<Q>());
    return value;
#endif
}
        
template<class Q>
    requires (is_pair<typename std::remove_cv<Q>::type>::value)
Q fromStr(const std::string& str)
{
    using namespace util;
    auto parts = parse_array_parts(truncate(str));
    if(parts.size() != 2) {
        std::string x = name<typename Q::first_type>();
        std::string y = name<typename Q::second_type>();
        throw std::invalid_argument("Illegal pair<"+x+", "+y+"> format ('"+str+"').");
    }
            
    auto x = Meta::fromStr<typename Q::first_type>(parts[0]);
    auto y = Meta::fromStr<typename Q::second_type>(parts[1]);
            
    return Q(x, y);
}
        
template<class Q>
    requires _clean_same<std::string, Q>
Q fromStr(const std::string& str)
{
    if((utils::beginsWith(str, '"') && utils::endsWith(str, '"'))
        || (utils::beginsWith(str, '\'') && utils::endsWith(str, '\'')))
        return util::unescape(/*utils::trim(*/str.substr(1, str.length()-2)/*)*/);
    return util::unescape(/*utils::trim(*/str/*)*/);
}
    
template<class Q>
    requires _clean_same<std::wstring, Q>
Q fromStr(const std::string& str)
{
    return s2ws(str);
}
        
template<class Q>
    requires (is_container<Q>::value || is_deque<Q>::value)
Q fromStr(const std::string& str)
{
    std::vector<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(str));
    for(auto &s : parts) {
        if(s.empty()) {
            ret.push_back(typename Q::value_type());
            throw std::invalid_argument("Empty value in '"+str+"'.");
        }
        else {
            auto v = Meta::fromStr<typename Q::value_type>(s);
            ret.push_back(v);
        }
    }
            
    return ret;
}
    
template<class Q>
    requires (!is_deque<Q>::value && is_queue<Q>::value)
Q fromStr(const std::string& str)
{
    std::vector<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(str));
    for(auto &s : parts) {
        if(s.empty()) {
            ret.push(typename Q::value_type());
            throw std::invalid_argument("Empty value in '"+str+"'.");
        }
        else {
            auto v = Meta::fromStr<typename Q::value_type>(s);
            ret.push(v);
        }
    }
            
    return ret;
}
        
template<class Q>
    requires (is_set<Q>::value)
Q fromStr(const std::string& str)
{
    std::set<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(str));
    for(auto &s : parts) {
        if(s.empty()) {
            ret.insert(typename Q::value_type());
            throw std::invalid_argument("Empty value in '"+str+"'.");
        }
        else {
            auto v = Meta::fromStr<typename Q::value_type>(s);
            ret.insert(v);
        }
    }
            
    return ret;
}
        
template<class Q>
    requires (is_map<Q>::value)
Q fromStr(const std::string& str)
{
    Q r;
            
    auto parts = util::parse_array_parts(util::truncate(str));
            
    for(auto &p: parts) {
        auto value_key = util::parse_array_parts(p, ':');
        if(value_key.size() != 2)
            throw std::invalid_argument("Illegal value/key pair: "+p);
        
        if constexpr(not utils::StringLike<typename Q::key_type>) {
            if(value_key[0].size() > 1
               && is_in(value_key[0].front(), '"', '\'')
               && value_key[0].front() == value_key[0].back())
            {
                value_key[0] = Meta::fromStr<std::string>(value_key[0]);
            }
        }
        
        auto x = Meta::fromStr<typename Q::key_type>(value_key[0]);
        try {
            if constexpr(not utils::StringLike<typename Q::mapped_type>) {
                if(value_key[1].size() > 1
                   && is_in(value_key[1].front(), '"', '\'')
                   && value_key[1].front() == value_key[1].back())
                {
                    value_key[1] = Meta::fromStr<std::string>(value_key[1]);
                }
            }
            
            auto y = Meta::fromStr<typename Q::mapped_type>(value_key[1]);
            r[x] = y;
        } catch(const std::logic_error&) {
            auto name = Meta::name<Q>();
            throw std::invalid_argument("Empty/illegal value in "+name+"['"+value_key[0]+"'] = '"+value_key[1]+"'.");
        }
    }
            
    return r;
}

// <_Meta::detail>
#pragma region _Meta::detail
namespace detail {

template <class F, typename... Args, size_t... Is>
auto transform_each_impl(const std::tuple<Args...>& t, F&& f, std::index_sequence<Is...>) {
    return std::make_tuple(
        f(Is, std::get<Is>(t) )...
    );
}

}
#pragma endregion _Meta::detail
// </_Meta::detail>
    
template <class F, typename... Args>
auto transform_each(const std::tuple<Args...>& t, F&& f) {
    return detail::transform_each_impl(
        t, std::forward<F>(f), std::make_index_sequence<sizeof...(Args)>{});
}
    
template<class Q>
    requires (is_instantiation<std::tuple, Q>::value)
Q fromStr(const std::string& str) {
    auto parts = util::parse_array_parts(util::truncate(str));
    //size_t i=0;
    if(parts.size() != std::tuple_size<Q>::value)
        throw illegal_syntax("tuple has "+std::to_string(parts.size())+" parts instead of " + std::to_string(std::tuple_size<Q>::value));
            
    Q tup;
    return transform_each(tup, [&](size_t i, auto&& obj) {
        return Meta::fromStr<decltype(obj)>(parts.at(i));
    });
}
        
template<class Q>
    requires _clean_same<cv::Mat, Q>
Q fromStr(const std::string&)
{
    throw std::invalid_argument("Not supported.");
}
        
template<class Q>
    requires _clean_same<bool, Q>
Q fromStr(const std::string& str)
{
    return str == "true" ? true : false;
}
        
template<class Q>
    requires _clean_same<cv::Range, Q>
Q fromStr(const std::string& str)
{
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2) {
        throw std::invalid_argument("Illegal cv::Range format: "+str);
    }
            
    int x = Meta::fromStr<int>(parts[0]);
    int y = Meta::fromStr<int>(parts[1]);
            
    return cv::Range(x, y);
}
        
/*template<typename ValueType, size_t N, typename _names>
Enum<ValueType, N, _names> fromStr(const std::string& str, const Enum<ValueType, N, _names>* =nullptr)
{
    return Enum<ValueType, N, _names>::get(Meta::fromStr<std::string>(str));
}
        
template<class Q>
    requires _clean_same<const Q&, decltype(Q::get(std::string_view()))>
const Q& fromStr(const std::string& str) {
    return Q::get(Meta::fromStr<std::string>(str));
}*/
        
/*template<class Q>
    requires _clean_same<Range<double>, Q>
Q fromStr(const std::string& str)
{
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2) {
        throw CustomException<std::invalid_argument>("Illegal Rangef format.");
    }
            
    auto x = Meta::fromStr<double>(parts[0]);
    auto y = Meta::fromStr<double>(parts[1]);
            
    return Range<double>(x, y);
}
    
template<class Q>
    requires _clean_same<Range<float>, Q>
Q fromStr(const std::string& str)
{
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2) {
        throw CustomException<std::invalid_argument>("Illegal Rangef format.");
    }
            
    auto x = Meta::fromStr<float>(parts[0]);
    auto y = Meta::fromStr<float>(parts[1]);
            
    return Rangef(x, y);
}
    
template<class Q>
    requires _clean_same<Range<long_t>, Q>
Q fromStr(const std::string& str)
{
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2) {
        throw CustomException<std::invalid_argument>("Illegal Rangel format.");
    }
            
    auto x = Meta::fromStr<long_t>(parts[0]);
    auto y = Meta::fromStr<long_t>(parts[1]);
            
    return Range(x, y);
}*/
        
template<class Q>
    requires (is_instantiation<cv::Size_, Q>::value)
Q fromStr(const std::string& str)
{
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2) {
        std::string x = name<typename Q::value_type>();
        throw std::invalid_argument("Illegal cv::Size_<"+x+"> format.");
    }
            
    auto x = Meta::fromStr<typename Q::value_type>(parts[0]);
    auto y = Meta::fromStr<typename Q::value_type>(parts[1]);
            
    return cv::Size_<typename Q::value_type>(x, y);
}
        
template<class Q>
    requires (is_instantiation<cv::Rect_, Q>::value)
Q fromStr(const std::string& str)
{
    using C = typename Q::value_type;
            
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 4) {
        std::string x = name<C>();
        throw std::invalid_argument("Illegal cv::Rect_<"+x+"> format.");
    }
            
    auto x = Meta::fromStr<C>(parts[0]);
    auto y = Meta::fromStr<C>(parts[1]);
    auto w = Meta::fromStr<C>(parts[2]);
    auto h = Meta::fromStr<C>(parts[3]);
            
    return Q(x, y, w, h);
}

}
#pragma endregion _Meta
// </_Meta>

// <Meta implementation>
#pragma region Meta implementation
namespace Meta {

template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return _Meta::name<typename cmn::remove_cvref<Q>::type>();
}
        
template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
inline std::string toStr(const Q& value, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return _Meta::toStr<typename cmn::remove_cvref<Q>::type>(value);
}

template<typename Q, typename K>
    requires is_instantiation<std::atomic, K>::value
inline std::string toStr(const Q& value) {
    return toStr(value.load());
}
        
template<typename Q, typename T>
inline T fromStr(const std::string& str, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return _Meta::fromStr<T>(str);
}
        
template<typename Q, typename T>
inline T fromStr(const std::string& str, const typename std::enable_if< std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return new typename std::remove_pointer<typename cmn::remove_cvref<Q>::type>(str);
}
        
template<typename Q>
inline std::string name(const typename std::enable_if< std::is_pointer<Q>::value && std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return "c_str";
}
        
template<typename Q>
inline std::string toStr(Q value, const typename std::enable_if< std::is_pointer<Q>::value && std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return Meta::toStr(std::string(value));
}
        
template<typename Q>
inline std::string name(const typename std::enable_if< std::is_pointer<Q>::value && !std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return Meta::name<typename std::remove_pointer<typename cmn::remove_cvref<Q>::type>::type>();
}
        
template<typename Q>
inline std::string toStr(Q value, const typename std::enable_if< std::is_pointer<Q>::value && !std::is_same<Q, const char*>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return "("+Meta::name<Q>()+"*)"+Meta::toStr<const typename std::remove_pointer<typename cmn::remove_cvref<Q>::type>::type&>(*value);
}

}
#pragma endregion Meta implementation
// </Meta implementation>

}

namespace cmn::utils {

template <size_t N>
constexpr auto lowercase(const char(&input)[N]) {
    ::cmn::util::ConstString_t result;
    for (size_t i = 0; i < N; ++i) {
        result[i] = lowercase_char(static_cast<char>(input[i]));
    }
    return result;
}

inline constexpr auto lowercase(const ::cmn::util::ConstString_t& input) {
    ::cmn::util::ConstString_t result;
    const auto L = input.view().length();
    for (size_t i = 0; i < L; ++i) {
        result[i] = lowercase_char(input[i]);
    }
    result[L] = 0;
    return result;
}

template<size_t N>
constexpr inline bool lowercase_equal_to(const std::string_view& A, const cmn::util::ConstexprString<N>& _B) {
    const auto B = _B.view();
    if (A.length() != B.length()) {
        return false;
    }
    if(A.length() > N)
        return false;

    // Compare each character, case-insensitively
    for (size_t i = 0; i < B.length(); ++i) {
        if (lowercase_char(static_cast<unsigned char>(A[i])) !=
            lowercase_char(static_cast<unsigned char>(B[i]))) {
            return false;
        }
    }

    return true;
}

}
