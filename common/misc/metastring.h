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

#include <misc/useful_concepts.h>

namespace cmn {

namespace utils {

template<template <typename ...> class T, typename Rt, typename... Args>
    requires is_instantiation<std::function, T<Rt(Args...)>>::value
std::string get_name(const T<Rt(Args...)>&);

void fast_fromstr(std::string& output, std::string_view sv);


}


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
public:
    static constexpr size_t Size = N;
private:
    std::array<char, N> _data{ 0 };
    size_t _length{ 0 };

public:
    constexpr size_t capacity() const noexcept { return N - 1u; }

    // Constructor from const char array
    template<size_t M>
        requires (M <= N)
    constexpr ConstexprString(const char (&arr)[M]) noexcept : _length(M - 1) {
        _length = M - 1;
        for (size_t i = 0; i < M - 1; ++i) {
            if(arr[i] == 0) {
                _length = i;
                break;
            }
            _data[i] = arr[i];
        }
    }

    // Constructor from std::array
    template<size_t M>
        requires (M <= N)
    constexpr ConstexprString(const std::array<char, M>& arr) noexcept : _length(M - 1) {
        _length = M - 1;
        for (size_t i = 0; i < M - 1; ++i) {
            if(arr[i] == 0) {
                _length = i;
                break;
            } else
                _data[i] = arr[i];
        }
    }

    // Constructor from another ConstexprString and applies a function
    template<size_t M, typename Func>
    constexpr ConstexprString(const ConstexprString<M>& other, Func&& func) noexcept {
        static_assert(M <= N, "Source string size exceeds destination capacity");
        _length = other.size();
        for (size_t i = 0; i < other.size(); ++i) {
            if(other[i] == 0) {
                break;
            }
            _data[i] = func(other[i]);
        }
        _data[_length] = 0;
    }

    // Constructor from const char array and applies a function
    template<size_t M, typename Func>
    constexpr ConstexprString(const char (&arr)[M], Func&& func) noexcept : _length(M - 1) {
        static_assert(M <= N, "Source string size exceeds destination capacity");
        _length = M - 1;
        for (size_t i = 0; i < M - 1; ++i) {
            if(arr[i] == 0) {
                _length = i;
                break;
            }
            _data[i] = func(arr[i]);
        }
    }

    constexpr const char* data() const { return _data.data(); }

    // Default constructor
    constexpr ConstexprString() = default;

    // Accessor to get std::string_view
    constexpr std::string_view view() const {
        return std::string_view(_data.data(), _length);
    }

    // Other utility methods as needed, e.g., size, operator[], etc.
    constexpr size_t size() const {
        return _length;
    }

    constexpr char operator[](size_t index) const {
        if (std::is_constant_evaluated()) {
            return _data[index];
        } else {
            if(index >= _length)
                throw std::out_of_range("Out of range access to ConstexprString.");
            return _data[index];
        }
    }

    constexpr bool operator==(const std::string_view& other) const noexcept {
        return other == view();
    }

    // Constructor from const char array
    template<size_t M>
    constexpr bool operator==(const char (&arr)[M]) const noexcept {
        return std::string_view(arr, M - 1) == view();
    }

    constexpr explicit operator std::string() const noexcept {
        return std::string(view());
    }

    constexpr operator std::string_view() const noexcept {
        return view();
    }

    // Append method
    template<size_t L>
    constexpr auto append(const ConstexprString<L>& other) const {
        static_assert(L <= N, "Resulting string exceeds capacity");

        ConstexprString<N> result;
        result._length = _length + other._length;

        for (size_t i = 0; i < _length; ++i) {
            result._data[i] = _data[i];
        }

        for (size_t j = 0; j < other._length; ++j) {
            result._data[_length + j] = other[j];
        }

        result._data[result._length] = 0;
        return result;
    }

    template<size_t M>
    constexpr auto append(const char (&arr)[M]) const {
        constexpr size_t arr_length = M - 1;
        static_assert(M < N, "Resulting string exceeds capacity");

        ConstexprString<N> result;
        result._length = _length + arr_length;

        for (size_t i = 0; i < _length; ++i) {
            result._data[i] = _data[i];
        }

        for (size_t j = 0; j < arr_length; ++j) {
            result._data[_length + j] = arr[j];
        }

        result._data[result._length] = 0;
        return result;
    }

    // Fill method
    template<char Filler>
    constexpr auto fill() const {
        ConstexprString<N> result;
        std::fill(result._data.begin(), result._data.end(), Filler);
        if constexpr(Filler == 0) {
            result._length = 0;
        } else
            result._length = N;
        return result;
    }

    // Apply method for function with only input character
    template <typename Func>
        requires (std::is_invocable_v<Func, char>
               || std::is_invocable_v<Func, size_t, char>)
    constexpr auto apply(Func&& func) const {
        ConstexprString<N> result;
        result._length = _length;
        
        for (size_t i = 0; i < _length; ++i) {
            if constexpr (std::is_invocable_v<Func, char>) {
                result._data[i] = func(_data[i]);
            } else {
                static_assert(std::is_invocable_v<Func, size_t, char>);
                result._data[i] = func(i, _data[i]);
            }
            
            if(result._data[i] == 0) {
                result._length = i;
                break;
            }
        }
        return result;
    }

    // Apply method over the entire capacity
    template <typename Func>
        requires (std::is_invocable_v<Func, char>
               || std::is_invocable_v<Func, size_t, char>)
    constexpr auto apply_to_capacity(Func&& func) const {
        ConstexprString<N> result;
        result._length = N;
        
        for (size_t i = 0; i < N; ++i) {
            if constexpr (std::is_invocable_v<Func, char>) {
                result._data[i] = func(_data[i]);
            } else {
                static_assert(std::is_invocable_v<Func, size_t, char>);
                result._data[i] = func(i, _data[i]);
            }
                
            if(result._data[i] == 0) {
                result._length = i;
                break;
            }
        }
        return result;
    }
};

using ConstString_t = ConstexprString<128>;

// Function to safely cast a value to int64_t
template <typename T>
constexpr bool is_within_int64_range(T value) {
    return value <= static_cast<T>(std::numeric_limits<int64_t>::max()) && value >= static_cast<T>(std::numeric_limits<int64_t>::min());
}

template <typename T>
requires std::floating_point<T>
constexpr ConstString_t to_string(T value) {
    std::array<char, 128> contents{0}; // Initialize array with null terminators
    
    //if (std::is_constant_evaluated()) {
        if (value != value) {
            /*const char nan[] = "nan";
            std::copy(std::begin(nan), std::end(nan) - 1, contents.data()); // -1 to exclude null terminator
            return contents;*/
            return "nan";
        }
    
        if (   value == std::numeric_limits<T>::infinity()
            || value == -std::numeric_limits<T>::infinity()
            || not is_within_int64_range(value))
        {
            /*const char* inf = (value < 0) ? "-inf" : "inf";
            std::copy(inf, inf + std::string_view(inf).length(), contents.data());
            return contents;*/
            if(value < 0) {
                return "-inf";
            } else
                return "inf";
        }

        /// the maximum number of pre-decimal point digits
        constexpr int max_digits = 50;
    
        // Extract integer and fractional parts
        int64_t integerPart = static_cast<int64_t>(value);
        T fractionalPart = value - integerPart;
        if(fractionalPart < 0)
            fractionalPart = -fractionalPart;

        // Convert integer part directly into 'contents'
        char* current = contents.data() + max_digits + 1; // Make room for the largest integer
        if (value < 0) {
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
        auto n = max_digits + 2 - (current - contents.data() + 1);
        //std::fill(contents.data(), contents.data() + contents.size(), 0);

        std::array<char, 128> copy{0};
        std::copy(current, current + n, copy.data());
        //std::fill(contents.data() + n, contents.data() + contents.size(), 0);
        
        // Convert fractional part
        char* fraction_start = copy.data() + n;
        *fraction_start = '.';
        char* fraction_current = fraction_start + 1;

        constexpr T tolerance{1e-4};
        constexpr uint8_t max_nines{2};
        char* nines_start = nullptr;
        uint8_t nines = 0;
    
        char* last_non_zero = 0;
        //constexpr int max_fractions_digits = std::numeric_limits<T>::digits10;
        /// THIS IS WRONG. But it makes them agree more.
        constexpr int max_fractions_digits = std::numeric_limits<float>::digits10;
    
        for (int i = 0; i < max_fractions_digits + max_nines; ++i) {
            fractionalPart *= 10;
            char digit = '0' + static_cast<int64_t>(fractionalPart);
            *fraction_current = digit;
            
            if(digit == '9' && i >= max_fractions_digits - 2) {
                if(not nines_start) {
                    nines_start = fraction_current;
                    nines = 0;
                }
                
                ++nines;
                
                if(nines > max_nines) {
                    static_assert(max_nines > 0);
                    /// this should be valid since we counted some nines
                    --nines_start;
                    
                    while(*nines_start >= '9' || nines_start == fraction_start) {
                        if(nines_start == fraction_start) {
                            last_non_zero = (fraction_start - 1);
                        } else {//if(nines_start < fraction_start) {
                            //last_non_zero = nullptr;
                            *nines_start = '0';
                        }
                        
                        if(nines_start == copy.data()) {
                            /// we have reached the beginning of the array. shift everything
                            /// up to the fraction point to zero, setting the
                            /// fraction point to zero to add a digit
                            //for(char * s = copy.data(); s <= fraction_point; ++s)
                            //    *s = '0';
                            //*nines_start = '0'; // set to zero so in the next step it increments to 1 since we add a digit
                            *fraction_start = '0';
                            last_non_zero = fraction_start;
                            break;
                        } else {
                            --nines_start;
                        }
                    }
                    
                    if(nines_start > fraction_start)
                    {
                        //assert(*nines_start == '0');
                        last_non_zero = nines_start;
                    }
                    *nines_start = *nines_start + 1;
                    break;
                }
            } else if(nines_start)
                nines_start = nullptr;
            
            if (digit != '0') {
                last_non_zero = fraction_current;
            }
            fraction_current++;
            fractionalPart -= static_cast<int64_t>(fractionalPart);
            
            if (fractionalPart < tolerance && fractionalPart > -tolerance) {
                break;  // Break when the fractional part is close enough to zero
            }
        }
    
    if(fraction_current - fraction_start >= max_fractions_digits) {
        *(fraction_start + max_fractions_digits) = 0;
    }

        if (last_non_zero) {
            *(last_non_zero + 1) = '\0'; // Trim the trailing zeros
        } else {
            *fraction_start = '\0'; // If the fractional part is all zeros, remove it.
        }
        
        return ConstString_t{copy};
    /*} else {
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
        auto result = cmn::to_chars(contents.data(), contents.data() + contents.size(), value, cmn::chars_format::fixed);
        if (result.ec == std::errc{}) {
            // Trim trailing zeros but leave at least one digit after the decimal point
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
            return ConstString_t{contents};
        } else {
            throw std::invalid_argument("Error converting floating point to string");
        }
    }*/
}

template<typename T>
requires std::integral<T>
constexpr ConstString_t to_string(T value) {
    //if (std::is_constant_evaluated()) {
        // For simplicity, we'll assume a maximum of 20 chars for int64_t (and its negative sign).
        std::array<char, 21> buffer{0};
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
        
        std::array<char, 21> result{0};
        std::copy(current, end, result.data());
        return ConstString_t{result}; //std::string(current, end - current);
    /*} else {
        char buffer[128] = {0};
        auto result = cmn::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec == std::errc{}) {
            *result.ptr = 0;
            return ConstString_t{buffer};
        } else {
            throw std::invalid_argument("Error converting to string");
        }
    }*/
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

inline std::string escape(std::string_view str) {
    auto _str = utils::find_replace(str, "\\", "\\\\");
    return utils::find_replace(_str, "\"", "\\\"");
}

inline std::string unescape(std::string_view str) {
    auto _str = utils::find_replace(str, "\\\"", "\"");
    return utils::find_replace(_str, "\\\\", "\\");
}

template<typename Str>
concept is_viewable = std::is_array_v<std::remove_cvref_t<Str>>
        || _is_dumb_pointer<std::remove_cvref_t<Str>>
        || std::same_as<std::remove_cvref_t<Str>, std::string_view>
        || (std::is_lvalue_reference_v<Str> &&
            _clean_same<std::remove_reference_t<Str>, std::string>);

auto parse_array_parts(cmn::StringLike auto&& str,
                       const char delimiter = ',')
{
    using Str = decltype(str);

    std::string_view sv;
    if constexpr(std::is_array_v<std::remove_cvref_t<Str>>
                 || _is_dumb_pointer<Str>)
    {
        sv = std::string_view(str);
    } else if constexpr(_clean_same<Str, std::string_view>) {
        sv = str;
    } else if constexpr(_clean_same<Str, std::wstring>) {
        static_assert(std::same_as<std::string, Str>, "");
    } else {
        sv = std::string_view(str);
    }

    // ---- choose element type ------------------------------------------------
    // We can safely hand out string_views only when the caller guarantees
    // that the underlying buffer will out‑live the views.  That is true for
    //   * character arrays / literals
    //   * raw C‑strings
    //   * std::string_view itself
    //   * *l‑value* std::string objects
    // Everything else (temporary std::string, std::string&& …) requires copies.
    static constexpr bool _safe_to_view =
        is_viewable<Str>;

    using Return_t = std::conditional_t<_safe_to_view, std::string_view, std::string>;

    // ---- split implementation ----------------------------------------------
    std::vector<Return_t> ret;
    std::stack<char> brackets;
    char   prev        = 0;
    size_t token_start = 0;
    
    if(sv.empty())
        return ret;

    const auto flush_token = [&](size_t end_index) {
        auto token_view = utils::trim(
            std::string_view(sv.data() + token_start, end_index - token_start));
        if constexpr (_safe_to_view)
            ret.emplace_back(token_view);
        else
            ret.emplace_back(std::string(token_view));
    };

    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        bool in_string = !brackets.empty() &&
                         (brackets.top() == '\'' || brackets.top() == '"');

        if (in_string) {
            if (prev != '\\' && c == brackets.top())
                brackets.pop();
        } else {
            switch (c) {
                case '(': case '[': case '{': case '"': case '\'':
                    brackets.push(c); break;
                case ']':
                    if (!brackets.empty() && brackets.top() == '[') brackets.pop();
                    break;
                case '}':
                    if (!brackets.empty() && brackets.top() == '{') brackets.pop();
                    break;
                case ')':
                    if (!brackets.empty() && brackets.top() == '(') brackets.pop();
                    break;
            }

            if (brackets.empty() && c == delimiter) {
                flush_token(i);
                token_start = i + 1;
            }
        }

        prev = (prev == '\\' && c == '\\') ? 0 : c;
    }

    // final token (handles trailing‑delimiter case, too)
    flush_token(sv.size());

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
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);

template<typename Q>
    requires is_instantiation<std::optional, Q>::value
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);

template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
        && (!is_instantiation<std::optional, Q>::value)
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);
        
template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
        && (not is_instantiation<std::function, Q>::value)
        && (not is_instantiation<std::optional, Q>::value)
inline std::string toStr(const Q& value, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* =nullptr);

template<typename Q>
    requires (is_instantiation<std::optional, Q>::value)
inline std::string toStr(const Q& value);
        
template<typename Q, typename T = typename cmn::remove_cvref<Q>::type>
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< !std::is_pointer<Q>::value
                 && (not is_instantiation<std::function, Q>::value)
                 && (not is_instantiation<std::optional, Q>::value),
                 typename cmn::remove_cvref<Q>::type >::type* =nullptr);


template<typename Q, typename T = typename cmn::remove_cvref<Q>::type>
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< !std::is_pointer<Q>::value
                 && (is_instantiation<std::optional, Q>::value), typename cmn::remove_cvref<Q>::type >::type* =nullptr);

template<typename Q>
    requires is_instantiation<std::function, Q>::value
inline Q fromStr(const std::string&, const Q* = nullptr) {
    throw std::runtime_error("Cannot generate function from string.");
}

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
template<class Q> std::string name(const typename std::enable_if< std::is_same<void, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "void"; }
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
template<class Q> std::string name(const typename std::enable_if< std::is_same<std::string_view, typename std::remove_cv<Q>::type>::value, Q >::type* =nullptr) { return "string_view"; }
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
    requires (is_instantiation<std::function, Q>::value)
std::string name() {
    return utils::get_name(Q{});
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
    std::string out;

    // Reserve some capacity if the container exposes size()
    if constexpr (requires(const Q& c) { c.size(); }) {
        out.reserve(obj.size() * 4 + 2);       // heuristic: average 4 bytes per element
    }
    
    bool first = true;
    out.push_back('[');

    for (const auto& elem : obj) {
        if (!first)
            out.push_back(',');
        first = false;
        out += Meta::toStr(elem);
    }

    out.push_back(']');
    return out;
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
            && (not _has_blocking_tostr_method<Q>)
            //&& (!std::convertible_to<Q, std::string>)
            //&& (!std::is_constructible_v<std::string, Q>)
            && (!_is_number<Q>)
std::string toStr(const Q& obj) {
    return obj.toStr();
}

template<class Q>
    requires _has_blocking_tostr_method<Q>
            //&& (!std::convertible_to<Q, std::string>)
            //&& (!std::is_constructible_v<std::string, Q>)
            && (!_is_number<Q>)
std::string toStr(const Q& obj) {
    return obj.toStr();//blocking_toStr();
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
    requires (std::unsigned_integral<typename std::remove_cv<Q>::type>
                || std::signed_integral<typename std::remove_cv<Q>::type>)
             && (!_clean_same<bool, Q>)
Q fromStr(cmn::StringLike auto&& str)
{
    using Str = decltype(str);
    std::string_view sv;
    
    if constexpr(std::is_array_v<std::remove_cvref_t<Str>>
                 || _is_dumb_pointer<Str>)
    {
        sv = std::string_view(str);
        
    } else if constexpr(_clean_same<Str, std::string_view>) {
        sv = str;
        
    } else if constexpr(_clean_same<Str, std::wstring>) {
        static_assert(std::same_as<std::string, Str>, "");
        
    } else {
        sv = std::string_view(str);
    }
    
    Q result;
    std::from_chars_result ec;
    
    if (!sv.empty() && str[0] == '\'' && sv.back() == '\'') {
        auto sub = sv.substr(1, sv.length() - 2);
        ec = std::from_chars(sub.data(), sub.data() + sub.size(), result);
    } else {
        ec = std::from_chars(sv.data(), sv.data() + sv.size(), result);
    }
    
    if(ec.ec != std::errc{}) {
        throw std::invalid_argument("Cannot convert \""+(std::string)sv+"\" to "+ Meta::name<Q>()+": "+util::to_readable_errc(ec.ec));
    }
    return result;
}
    
template<class Q>
    requires _has_fromstr_method<Q>
Q fromStr(cmn::StringLike auto&& str) {
    return Q::fromStr(std::forward<decltype(str)>(str));
}
        
template<class Q>
    requires std::floating_point<typename std::remove_cv<Q>::type>
Q fromStr(cmn::StringLike auto&& str)
{
#if defined(__clang__)
    std::string_view sv(str);
    if (!sv.empty() && sv.front() == '\'' && sv.back() == '\'')
        sv = sv.substr(1, sv.size() - 2);

    const char* begin = sv.data();
    char* endPtr = nullptr;

    errno = 0;
    double val = std::strtod(begin, &endPtr);

    if (endPtr != begin + static_cast<ptrdiff_t>(sv.size()) || errno == ERANGE) {
        std::ostringstream oss;
        oss << "Cannot convert value: '" << sv << "' to " << Meta::name<Q>() << ". ";
        oss << "errno=" << errno << " (" << strerror(errno) << ") ";
        oss << "parsed up to offset " << (endPtr - begin) << "/" << sv.size() << ". ";
        if (endPtr != begin + static_cast<ptrdiff_t>(sv.size())) {
            oss << "Stopped parsing at: '";
            if (endPtr < begin + static_cast<ptrdiff_t>(sv.size()))
                oss << std::string_view(endPtr, (begin + static_cast<ptrdiff_t>(sv.size())) - endPtr);
            else
                oss << "(out of bounds)";
            oss << "'. ";
        }
        
        auto str = oss.str();
        printf("[error] %s\n", str.c_str());
        throw std::runtime_error(str);
    }
        
    return static_cast<Q>(val);
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
Q fromStr(cmn::StringLike auto&& str)
{
    using namespace util;
    auto parts = parse_array_parts(truncate(std::string_view(str)));
    if(parts.size() != 2) {
        std::string x = name<typename Q::first_type>();
        std::string y = name<typename Q::second_type>();
        throw std::invalid_argument("Illegal pair<"+x+", "+y+"> format ('"+(std::string)str+"').");
    }
            
    auto x = Meta::fromStr<typename Q::first_type>(parts[0]);
    auto y = Meta::fromStr<typename Q::second_type>(parts[1]);
            
    return Q(x, y);
}
        
template<class Q>
    requires _clean_same<std::string, Q>
Q fromStr(cmn::StringLike auto&& str)
{
    if((utils::beginsWith(str, '"') && utils::endsWith(str, '"'))
        || (utils::beginsWith(str, '\'') && utils::endsWith(str, '\'')))
        return util::unescape(/*utils::trim(*/str.substr(1, str.length()-2)/*)*/);
    return util::unescape(/*utils::trim(*/str/*)*/);
}
    
template<class Q>
    requires _clean_same<std::wstring, Q>
Q fromStr(cmn::StringLike auto&& str)
{
    return s2ws(str);
}
        
template<class Q>
    requires (is_container<Q>::value || is_deque<Q>::value)
Q fromStr(cmn::StringLike auto&& str)
{
    std::vector<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
    for(auto &s : parts) {
        if(s.empty()) {
            ret.push_back(typename Q::value_type());
            throw std::invalid_argument("Empty value in '"+std::string(str)+"'.");
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
Q fromStr(cmn::StringLike auto&& str)
{
    std::vector<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
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
Q fromStr(cmn::StringLike auto&& str)
{
    std::set<typename Q::value_type> ret;
    std::queue<char> brackets;
    std::stringstream value;
            
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
    for(auto &s : parts) {
        if(s.empty()) {
            ret.insert(typename Q::value_type());
            throw std::invalid_argument("Empty value in '"+(std::string)str+"'.");
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
Q fromStr(cmn::StringLike auto&& str)
{
    Q r;
            
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
            
    for(std::string_view p : parts) {
        auto value_key = util::parse_array_parts(p, ':');
        if(value_key.size() != 2)
            throw std::invalid_argument("Illegal value/key pair: "+(std::string)p);
        
        if constexpr(not cmn::StringLike<typename Q::key_type>) {
            if(value_key[0].size() > 1
               && is_in(value_key[0].front(), '"', '\'')
               && value_key[0].front() == value_key[0].back())
            {
                value_key[0] = Meta::fromStr<std::string>(value_key[0]);
            }
        }
        
        auto x = Meta::fromStr<typename Q::key_type>(value_key[0]);
        try {
            if constexpr(not cmn::StringLike<typename Q::mapped_type>) {
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
            throw std::invalid_argument("Empty/illegal value in "+name+"['"+(std::string)value_key[0]+"'] = '"+(std::string)value_key[1]+"'.");
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
Q fromStr(cmn::StringLike auto&& str) {
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
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
Q fromStr(cmn::StringLike auto&& str)
{
    return str == "true" ? true : false;
}
        
template<class Q>
    requires _clean_same<cv::Range, Q>
Q fromStr(cmn::StringLike auto&& str)
{
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
    if(parts.size() != 2) {
        throw std::invalid_argument("Illegal cv::Range format: "+str);
    }
            
    int x = Meta::fromStr<int>(parts[0]);
    int y = Meta::fromStr<int>(parts[1]);
            
    return cv::Range(x, y);
}
        
/*template<typename ValueType, size_t N, typename _names>
Enum<ValueType, N, _names> fromStr(cmn::StringLike auto&& str, const Enum<ValueType, N, _names>* =nullptr)
{
    return Enum<ValueType, N, _names>::get(Meta::fromStr<std::string>(str));
}
        
template<class Q>
    requires _clean_same<const Q&, decltype(Q::get(std::string_view()))>
const Q& fromStr(cmn::StringLike auto&& str) {
    return Q::get(Meta::fromStr<std::string>(str));
}*/
        
/*template<class Q>
    requires _clean_same<Range<double>, Q>
Q fromStr(cmn::StringLike auto&& str)
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
Q fromStr(cmn::StringLike auto&& str)
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
Q fromStr(cmn::StringLike auto&& str)
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
Q fromStr(cmn::StringLike auto&& str)
{
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
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
Q fromStr(cmn::StringLike auto&& str)
{
    using C = typename Q::value_type;
            
    auto parts = util::parse_array_parts(util::truncate(std::string_view(str)));
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
        && (!is_instantiation<std::optional, Q>::value)
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return _Meta::name<typename cmn::remove_cvref<Q>::type>();
}

template<typename Q>
    requires is_instantiation<std::optional, Q>::value
inline std::string name(const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return "optional<"+ _Meta::name<typename cmn::remove_cvref<typename Q::value_type>::type>() + ">";
}
        
template<typename Q>
    requires (!std::is_base_of<std::exception, Q>::value)
        && (!is_instantiation<std::atomic, Q>::value)
        && (not is_instantiation<std::function, Q>::value)
        && (not is_instantiation<std::optional, Q>::value)
inline std::string toStr(const Q& value, const typename std::enable_if< !std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return _Meta::toStr<typename cmn::remove_cvref<Q>::type>(value);
}

template<typename Q>
    requires (is_instantiation<std::optional, Q>::value)
inline std::string toStr(const Q& value) {
    if(value.has_value()) {
        using ValueType = cmn::remove_cvref_t<typename cmn::remove_cvref_t<Q>::value_type>;
        return _Meta::toStr<ValueType>(value.value());
    }
    return "null";
}

template<typename Q, typename K>
    requires is_instantiation<std::atomic, K>::value
inline std::string toStr(const Q& value) {
    return toStr(value.load());
}

template<typename Q>
    requires is_instantiation<std::function, Q>::value
inline std::string toStr(const Q& val) {
    return utils::get_name(val);
}
        
template<typename Q, typename T>
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< !std::is_pointer<Q>::value && (not is_instantiation<std::function, Q>::value) && (not is_instantiation<std::optional, Q>::value), typename cmn::remove_cvref<Q>::type >::type*) {
    return _Meta::fromStr<T>(std::forward<decltype(str)>(str));
}

template<typename Q, typename T>
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< !std::is_pointer<Q>::value
                 && (is_instantiation<std::optional, Q>::value), typename cmn::remove_cvref<Q>::type >::type*) {
    if(str == "null")
        return std::nullopt;
    return _Meta::fromStr<typename T::value_type>(std::forward<decltype(str)>(str));
}

template<typename Q, typename T>
inline T fromStr(cmn::StringLike auto&& str, const typename std::enable_if< std::is_pointer<Q>::value, typename cmn::remove_cvref<Q>::type >::type* ) {
    return new typename std::remove_pointer<typename cmn::remove_cvref<Q>::type>(std::forward<decltype(str)>(str));
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
    return ::cmn::util::ConstString_t(input, [](char c) { return lowercase_char(static_cast<int>(c)); });
}

inline constexpr auto lowercase(const ::cmn::util::ConstString_t& input) {
    return ::cmn::util::ConstString_t(input, [](char c) { return lowercase_char(static_cast<int>(c)); });
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

template<template <typename ...> class T, typename Rt, typename... Args>
    requires is_instantiation<std::function, T<Rt(Args...)>>::value
std::string get_name(const T<Rt(Args...)>&) {
    std::string ss;
    bool first = true;
    ((ss = ss + (first ? "" : ", ") + Meta::name<Args>(), first = false), ...);
    return "function<"+Meta::name<Rt>()+"("+ss+")>";
}

}
