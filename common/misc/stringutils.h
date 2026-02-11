#ifndef _STRINGUTILS_H
#define _STRINGUTILS_H

#include <string>
#include <vector>
#include <cctype>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <codecvt>
#include <type_traits>
#include <unicode/uchar.h>   // For u_isWhitespace
#include <unicode/utf8.h>    // For U8_NEXT and U8_PREV

namespace cmn::utils {

// Concept to identify valid needle types
template<typename T>
concept NeedleLike = std::is_same_v<std::remove_cvref_t<T>, char> ||
                     StringLike<T>;

template<typename T>
struct is_const_lvalue_ref : std::false_type {};

template<typename T>
struct is_const_lvalue_ref<const T&> : std::true_type {};

template<StringLike Str>
[[nodiscard]] constexpr std::string_view string_like_view(const Str& str) noexcept {
    using StrDecayed = std::remove_cvref_t<Str>;

    if constexpr(std::is_same_v<StrDecayed, const char*>) {
        return str == nullptr ? std::string_view{} : std::string_view{str};
    } else if constexpr(std::is_array_v<StrDecayed>) {
        static_assert(std::is_same_v<std::remove_cv_t<std::remove_extent_t<StrDecayed>>, char>,
                      "StringLike arrays must be char arrays.");
        constexpr auto extent = std::extent_v<StrDecayed>;
        if constexpr(extent == 0u) {
            return std::string_view{};
        } else {
            const size_t len = str[extent - 1u] == '\0' ? extent - 1u : extent;
            return std::string_view(str, len);
        }
    } else {
        return std::string_view{str};
    }
}

template<StringLike Str>
bool beginsWith(const Str& str, char c) {
    const std::string_view sv_str = string_like_view(str);
    return !sv_str.empty() && sv_str[0] == c;
}

template<StringLike Str>
bool endsWith(const Str& str, char c) {
    const std::string_view sv_str = string_like_view(str);
    return !sv_str.empty() && sv_str.back() == c;
}

template<StringLike Str, StringLike Needle>
bool beginsWith(const Str& str, const Needle& needle) {
    std::string_view sv_str = string_like_view(str);
    std::string_view sv_needle = string_like_view(needle);
    return sv_str.substr(0, sv_needle.size()) == sv_needle;
}

template<StringLike Str, StringLike Needle>
bool endsWith(const Str& str, const Needle& needle) {
    std::string_view sv_str = string_like_view(str);
    std::string_view sv_needle = string_like_view(needle);
    return sv_str.size() >= sv_needle.size() &&
           sv_str.substr(sv_str.size() - sv_needle.size()) == sv_needle;
}

/**
 * Finds a given needle inside the \p str given as first parameter.
 * Case-sensitive.
 * @param str haystack
 * @param needle the needle
 * @return true if it is found
 */
template<StringLike Str, NeedleLike Needle>
[[nodiscard]] constexpr bool contains(const Str& str, const Needle& needle) noexcept {
    const std::string_view sv_str = string_like_view(str);
    if(sv_str.empty())
        return false;

    if constexpr(std::is_same_v<std::remove_cvref_t<Needle>, char>) {
        return sv_str.find(needle) != std::string::npos;
    } else {
        const std::string_view sv_needle = string_like_view(needle);
        if(sv_needle.empty())
            return false;
        return sv_str.find(sv_needle) != std::string::npos;
    }
}

//! find and replace string in another string
/**
 * @param str the haystack
 * @param oldStr the needle
 * @param newStr replacement of needle
 * @return str with all occurences of needle replaced by newStr
 */
std::wstring find_replace(const std::wstring& str, const std::wstring& oldStr, const std::wstring& newStr);
std::string find_replace(std::string_view str, std::string_view oldStr, std::string_view newStr);

inline std::string find_replace(
    const std::string_view& subject,
    const std::vector<std::pair<std::string_view, std::string_view>>& search_replace_pairs) {

    if (subject.empty() || search_replace_pairs.empty()) {
        return std::string(subject);
    }

    std::string result;
    result.reserve(subject.size());

    auto subject_view = std::string_view(subject);

    for (size_t i = 0; i < subject_view.size();) {
        bool match_found = false;
        for (const auto& [search, replace] : search_replace_pairs) {
            auto search_view = std::string_view(search);
            if (subject_view.substr(i, search_view.size()) == search_view) {
                result.append(replace);
                i += search_view.size();
                match_found = true;
                break;  // Match found, no need to search further.
            }
        }

        if (!match_found) {
            result += subject_view[i++];
        }
    }

    return result;
}

inline bool isWhitespaceOrAdditional(UChar32 c) {
    return u_hasBinaryProperty(c, UCHAR_WHITE_SPACE) ||
        c == 0x200B || // ZERO WIDTH SPACE
        c == 0x2060 || // WORD JOINER
        c == 0xFEFF;   // ZERO WIDTH NO-BREAK SPACE (BOM)
}

enum class TrimDirection { Left, Right };

template<StringLike Str, TrimDirection Direction>
auto trim_impl(Str&& s) {
    using StrDecayed = std::remove_cvref_t<Str>;

    if (s.empty()) {
        if constexpr (std::is_same_v<StrDecayed, std::string>) {
            return std::forward<Str>(s);
        } else {
            return std::string_view{};
        }
    }

    const char* data = s.data();
    int32_t length = static_cast<int32_t>(s.size());
    int32_t start = 0;
    int32_t end = length;

    if constexpr (Direction == TrimDirection::Left) {
        // Left trim
        int32_t offset = start;
        while (offset < end) {
            UChar32 c;
            U8_NEXT(data, offset, end, c);
            if (c < 0) {
                // Invalid character, stop trimming
                break;
            }
            if (isWhitespaceOrAdditional(c)) {
                // Whitespace character, move start forward
                start = offset;
            } else {
                // Non-whitespace character found, stop
                break;
            }
        }
    } else {
        // Right trim
        int32_t offset = end;
        while (offset > start) {
            UChar32 c;
            U8_PREV(data, start, offset, c);
            if (c < 0) {
                // Invalid character, stop trimming
                break;
            }
            if (isWhitespaceOrAdditional(c)) {
                // Whitespace character, move end backward
                end = offset;
            } else {
                // Non-whitespace character found, stop
                break;
            }
        }
    }

    if constexpr ((std::is_rvalue_reference_v<Str&&> ||
                   is_const_lvalue_ref<Str>::value) &&
                  std::is_same_v<StrDecayed, std::string>) {
        // Create a new string and return it
        return std::string(data + start, end - start);
    } else if constexpr (std::is_same_v<StrDecayed, std::string>) {
        // Modify the original string in place
        s.erase(end, length - end);      // Erase from 'end' to the end
        s.erase(0, start);               // Erase from the start to 'start'
        return std::forward<Str>(s);
    } else {
        // For string_view and const char*
        if (start >= end) {
            return std::string_view{};
        }
        return std::string_view(data + start, end - start);
    }
}

template<StringLike Str>
auto ltrim(Str&& s) {
    return trim_impl<Str, TrimDirection::Left>(std::forward<Str>(s));
}

template<StringLike Str>
auto rtrim(Str&& s) {
    return trim_impl<Str, TrimDirection::Right>(std::forward<Str>(s));
}

template<StringLike Str>
auto trim(Str&& s) {
    return rtrim(ltrim(std::forward<Str>(s)));
}

template<typename Char = char>
constexpr inline Char lowercase_char(Char ch) {
    return (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
}
    
template<size_t len_cstr>
constexpr inline bool lowercase_equal_to(const std::string_view& str_view, const char(&cstr)[len_cstr]) {
    // First check lengths; if they're different, the strings can't be equal
    size_t len_view = str_view.length();
    if (len_view != len_cstr - 1u) {
        return false;
    }

    // Compare each character, case-insensitively
    for (size_t i = 0; i < len_view; ++i) {
        if (lowercase_char(static_cast<unsigned char>(str_view[i])) !=
            lowercase_char(static_cast<unsigned char>(cstr[i]))) {
            return false;
        }
    }

    return true;
}

constexpr inline bool lowercase_equal_to(std::string_view str_view, std::string_view other) {
    // First check lengths; if they're different, the strings can't be equal
    size_t len_view = str_view.length();
    size_t len_cstr = other.length();
    
    if (len_view != len_cstr) {
        return false;
    }

    // Compare each character, case-insensitively
    for (size_t i = 0; i < len_view; ++i) {
        if (lowercase_char(static_cast<unsigned char>(str_view[i])) !=
            lowercase_char(static_cast<unsigned char>(other[i]))) {
            return false;
        }
    }

    return true;
}

template<typename Str>
    requires (not std::is_array_v<std::remove_reference_t<Str>>)
        && (not std::is_pointer_v<std::remove_reference_t<Str>>)
auto lowercase(Str const& original) {
    using StrDecayed = std::remove_cvref_t<Str>;
    using CharType = typename StrDecayed::value_type;
    
    std::basic_string<CharType> result(original.begin(), original.end());

#ifndef WIN32
    std::transform(result.begin(), result.end(), result.begin(), (int(*)(int))std::tolower);
#else
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
#endif

    return result;
}

template<typename CStr>
    requires (not std::is_array_v<std::remove_reference_t<CStr>>)
        && (std::same_as<const char*, CStr> || std::same_as<const wchar_t*, CStr>)
auto lowercase(CStr original) {
    return utils::lowercase(std::basic_string_view{original});
}

template<typename Str>
auto uppercase(Str const& original) {
    using StrDecayed = std::remove_cvref_t<Str>;
    using CharType = typename StrDecayed::value_type;
    
    std::basic_string<CharType> result(original.begin(), original.end());

#ifndef WIN32
    std::transform(result.begin(), result.end(), result.begin(), (int(*)(int))std::toupper);
#else
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
#endif

    return result;
}

template<typename Char>
auto uppercase(const Char* original) {
    std::basic_string<Char> str(original);

#ifndef WIN32
    std::transform(str.begin(), str.end(), str.begin(), (int(*)(int))std::toupper);
#else
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
#endif

    return str;
}

// repeats a string N times
std::string repeat(const std::string& s, size_t N);

inline bool is_number_string(std::string_view sv) {
    for(auto a : sv) {
        if(a > '9' || a < '0')
            return false;
    }
    return true;
}

/**
 * Splits a string into substrings using a delimiter character.
 *
 * @param s The input string to split.
 * @param c The delimiter character to split on.
 * @param skip_empty Whether to skip empty substrings.
 *                   If true, consecutive delimiters will be skipped and no empty substrings will be added to the result vector.
 *                   If false, empty substrings will be added to the result vector for each consecutive delimiter.
 * @param trim Whether to remove whitespace characters from each substring.
 *
 * @return A vector of substrings, with each substring being an element of the vector.
 */
std::vector<std::string_view> split(std::string_view const& s, char c, bool skip_empty = false, bool trim = false);
std::vector<std::string> split(std::string const& s, char c, bool skip_empty = false, bool trim = false);
std::vector<std::basic_string_view<typename std::string::value_type>> split(std::string & s, char c, bool skip_empty = false, bool trim = false);
template<typename Str>
    requires std::is_same_v<const char*, std::remove_cvref_t<Str>>
                || std::is_array_v<std::remove_reference_t<Str>>
std::vector<std::string_view> split(Str s, char c, bool skip_empty = false, bool trim = false) {
    return split(std::string_view(s), c, skip_empty, trim);
}

/**
 * Splits a wide string into substrings using a delimiter character.
 *
 * @param s The input wide string to split.
 * @param c The delimiter character to split on.
 * @param skip_empty Whether to skip empty substrings.
 *                   If true, consecutive delimiters will be skipped and no empty substrings will be added to the result vector.
 *                   If false, empty substrings will be added to the result vector for each consecutive delimiter.
 * @param trim Whether to remove whitespace characters from each substring.
 *
 * @return A vector of substrings, with each substring being an element of the vector.
 */
std::vector<std::wstring> split(std::wstring const& s, char c, bool skip_empty = false, bool trim = false);
std::vector<std::basic_string_view<typename std::wstring::value_type>> split(std::wstring& s, char c, bool skip_empty = false, bool trim = false);

// Helper trait to detect string_view-like types
namespace detail {
    template<typename T>
    constexpr bool is_string_view_like_v = std::is_convertible_v<T, std::string_view>;
    
    template<typename T, typename = void>
    struct is_iterable_of_string_view : std::false_type {};
    
    template<typename T>
    struct is_iterable_of_string_view<T, std::void_t<decltype(std::begin(std::declval<T>())), decltype(std::end(std::declval<T>())), typename T::value_type>> : std::bool_constant<is_string_view_like_v<typename T::value_type>> {};
}

// Variadic overload – supports string_view-like values and containers of them
template<class... Args>
[[nodiscard]] std::string concat_views(Args&&... args) {
    using namespace detail;
    
    // Accumulate total size
    std::size_t total = 0;
    auto count_size = [&](auto&& arg) {
        using ArgT = std::remove_cvref_t<decltype(arg)>;
        if constexpr (is_string_view_like_v<ArgT>) {
            total += std::string_view{std::forward<decltype(arg)>(arg)}.size();
        } else if constexpr (is_iterable_of_string_view<ArgT>::value) {
            for (const auto& v : arg) {
                total += std::string_view{v}.size();
            }
        } else {
            static_assert(is_string_view_like_v<ArgT> || is_iterable_of_string_view<ArgT>::value, "Arguments must be string_view-like or iterable of string_view-like");
        }
    };
    (count_size(std::forward<Args>(args)), ...);
    
    std::string result(total, '\0');
    char* write = result.data();
    auto copy_one = [&](const auto& v) {
        std::string_view sv{v};
        std::memcpy(write, sv.data(), sv.size());
        write += sv.size();
    };
    auto do_copy = [&](auto&& arg) {
        using ArgT = std::remove_cvref_t<decltype(arg)>;
        if constexpr (is_string_view_like_v<ArgT>) {
            copy_one(arg);
        } else if constexpr (is_iterable_of_string_view<ArgT>::value) {
            for (const auto& v : arg) {
                copy_one(v);
            }
        }
    };
    (do_copy(std::forward<Args>(args)), ...);
    return result;
}

inline std::string ShortenText(std::string_view text, size_t maxLength, double positionPercent = 0.5, std::string_view shortenSymbol = "…") {
    if (text.length() <= maxLength) {
        return (std::string)text;
    }

    size_t shortenPosition = static_cast<size_t>(positionPercent * maxLength);
    // Ensure that the shorten position is within the valid range
    if (shortenPosition > maxLength - 1) {
        shortenPosition = maxLength - 1;
    }

    return concat_views(text.substr(0, shortenPosition), shortenSymbol, text.substr(text.length() - (maxLength - shortenPosition - 1)));
}


}

/**
 * @brief Converts a narrow string to a wide string.
 *
 * @param str The input narrow string.
 * @return A wide string representation of the input string.
 * @throws std::runtime_error If an invalid or incomplete multibyte sequence is encountered.
 */
[[nodiscard]] std::wstring s2ws(const std::string& str);

/**
 * @brief Converts a wide string to a narrow string.
 *
 * @param wstr The input wide string.
 * @return A narrow string representation of the input wide string.
 * @throws std::runtime_error If an invalid wide character is encountered.
 */
[[nodiscard]] std::string ws2s(const std::wstring& wstr);

/**
 * @brief Converts a UTF-8 encoded string to a wide string.
 *
 * @param utf8Str The input UTF-8 encoded string.
 * @return A wide string representation of the input UTF-8 string.
 * @throws std::runtime_error If an invalid or incomplete multibyte sequence is encountered.
 */
[[nodiscard]] std::wstring utf8ToWide(const std::u8string_view& utf8Str);

/**
 * @brief Converts a wide string to a UTF-8 encoded string.
 *
 * @param wideStr The input wide string.
 * @return A UTF-8 encoded string representation of the input wide string.
 * @throws std::runtime_error If an invalid wide character is encountered.
 */
[[nodiscard]] std::u8string wideToUtf8(const std::wstring_view& wideStr);

// Define a struct to store the preprocessed data.
struct PreprocessedData {
    // docFrequency stores the number of documents each term appears in.
    // For instance, if the term "apple" appears in 3 different documents,
    // docFrequency["apple"] would be 3.
    std::unordered_map<std::string, int> docFrequency;
    
    // termVectors contains a vector of term-frequency-inverse-document-frequency
    // (TF-IDF) vectors for each document in the corpus.
    // Each unordered_map represents the TF-IDF vector for a document,
    // with the keys being the terms and the values being the TF-IDF scores.
    std::vector<std::unordered_map<std::string, double>> termVectors;
    
    std::unordered_map<std::string, double> termImportance;
    std::vector<std::vector<std::string>> tokenizedCorpus; // To store tokenized documents
    
    // empty is a utility function that checks whether the PreprocessedData
    // object has any data. It returns true if termFrequency is empty.
    bool empty() const { return docFrequency.empty(); }
    
    // Function to get a string representation of the structure
    std::string toStr() const;

    // Static function to return the class name as a string
    static std::string class_name() {
        return "PreprocessedData";
    }
};

// Define a struct to store the preprocessed data.
struct PreprocessedDataWithDocs {
    // docFrequency stores the number of documents each term appears in.
    // For instance, if the term "apple" appears in 3 different documents,
    // docFrequency["apple"] would be 3.
    std::unordered_map<std::string, int> docFrequency;
    std::unordered_map<std::string, double> termImportance; // smoothed IDF
    
    std::vector<std::vector<std::string>> tokenizedNames; // To store tokenized documents
    std::vector<std::vector<std::string>> tokenizedDocs; // To store tokenized documents
    
    // termVectors contains a vector of term-frequency-inverse-document-frequency
    // (TF-IDF) vectors for each document in the corpus.
    // Each unordered_map represents the TF-IDF vector for a document,
    // with the keys being the terms and the values being the TF-IDF scores.
    std::vector<std::unordered_map<std::string, double>> nameVectors;
    std::vector<std::unordered_map<std::string, double>> docVectors;
    
    // empty is a utility function that checks whether the PreprocessedData
    // object has any data. It returns true if termFrequency is empty.
    bool empty() const { return docFrequency.empty(); }
    
    // Function to get a string representation of the structure
    std::string toStr() const;

    // Static function to return the class name as a string
    static std::string class_name() {
        return "PreprocessedDataWithDocs";
    }
};

PreprocessedData preprocess_corpus(const std::vector<std::string>& corpus);
PreprocessedDataWithDocs preprocess_corpus(const std::vector<std::string>& corpus, const std::vector<std::string>& docs);

[[deprecated]] std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus);
std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus, const PreprocessedData&);
std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus, const std::vector<std::string>& docs, const PreprocessedDataWithDocs&);

namespace cmn::utils {

template<cmn::StringLike Str>
std::string strip_html(const Str& input);

extern template std::string strip_html<std::string>(const std::string& input);
extern template std::string strip_html<std::string_view>(const std::string_view& input);
extern template std::string strip_html<const char*>(const char* const& input);
    
}

#endif
