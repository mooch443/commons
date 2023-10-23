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

namespace utils {

    template<typename Str>
    bool beginsWith(const Str& str, char c) {
        return !str.empty() && str.front() == c;
    }

    template<typename Str>
    bool endsWith(const Str& str, char c) {
        return !str.empty() && str.back() == c;
    }

    template<typename Str, typename Needle>
    bool beginsWith(const Str& str, const Needle& needle) {
        std::string_view sv_str(str);
        std::string_view sv_needle(needle);
        return sv_str.substr(0, sv_needle.size()) == sv_needle;
    }

    template<typename Str, typename Needle>
    bool endsWith(const Str& str, const Needle& needle) {
        std::string_view sv_str(str);
        std::string_view sv_needle(needle);
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
	bool contains(const std::string &str, const std::string &needle);
    bool contains(const std::string &str, const char &needle);
    bool contains(const std::wstring &str, const std::wstring &needle);
    bool contains(const std::wstring &str, const wchar_t &needle);
    
    //! find and replace string in another string
    /**
     * @param str the haystack
     * @param oldStr the needle
     * @param newStr replacement of needle
     * @return str with all occurences of needle replaced by newStr
     */
    std::string find_replace(const std::string& str, const std::string& oldStr, const std::string& newStr);
    std::wstring find_replace(const std::wstring& str, const std::wstring& oldStr, const std::wstring& newStr);

    std::string find_replace(const std::string& str, std::vector<std::tuple<std::string, std::string>>);
    
template<typename Str>
concept StringLike = std::is_same_v<std::remove_cvref_t<Str>, std::string> ||
                      std::is_same_v<std::remove_cvref_t<Str>, const char*> ||
                      std::is_same_v<std::remove_cvref_t<Str>, std::string_view> ||
                      std::is_array_v<std::remove_cvref_t<Str>>;

template<typename T>
struct is_const_lvalue_ref : std::false_type {};

template<typename T>
struct is_const_lvalue_ref<const T&> : std::true_type {};

template<StringLike Str>
auto ltrim(Str&& s) {
    using StrDecayed = std::remove_cvref_t<Str>;
    using CharType = typename StrDecayed::value_type;
    auto pred = [](CharType c) -> bool { return !std::isspace(c); };
    
    if constexpr((std::is_rvalue_reference_v<Str&&>
                  || is_const_lvalue_ref<Str>::value)
                 && std::is_same_v<StrDecayed, std::string>)
    {
        auto copy = s;
        copy.erase(copy.begin(), std::find_if(copy.begin(), copy.end(), pred));
        return copy;
        
    } else if constexpr (std::is_same_v<StrDecayed, std::string>) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), pred));
        return std::forward<Str>(s);
    } else {
        auto start = std::find_if(s.begin(), s.end(), pred);
        return std::string_view(&*start, std::distance(start, s.end()));
    }
}

template<StringLike Str>
auto rtrim(Str&& s) {
    using StrDecayed = std::remove_cvref_t<Str>;
    using CharType = typename StrDecayed::value_type;
    auto pred = [](CharType c) -> bool { return !std::isspace(c); };
    
    if constexpr((std::is_rvalue_reference_v<Str&&>
                  || is_const_lvalue_ref<Str>::value)
                 && std::is_same_v<StrDecayed, std::string>)
    {
        auto copy = s;
        copy.erase(std::find_if(copy.rbegin(), copy.rend(), pred).base(), copy.end());
        return copy;
        
    } else if constexpr (std::is_same_v<StrDecayed, std::string>) {
        s.erase(std::find_if(s.rbegin(), s.rend(), pred).base(), s.end());
        return std::forward<Str>(s);
        
    } else {
        auto end = std::find_if(s.rbegin(), s.rend(), pred).base();
        return std::string_view(&*s.begin(), std::distance(s.begin(), end));
    }
}

template<StringLike Str>
auto trim(Str&& s) {
    return rtrim(ltrim(std::forward<Str>(s)));
}
    
inline bool lowercase_equal_to(const std::string_view& str_view, const char* cstr) {
    // First check lengths; if they're different, the strings can't be equal
    size_t len_view = str_view.length();
    size_t len_cstr = std::strlen(cstr); // include <cstring> for std::strlen
    if (len_view != len_cstr) {
        return false;
    }

    // Compare each character, case-insensitively
    for (size_t i = 0; i < len_view; ++i) {
        if (std::tolower(static_cast<unsigned char>(str_view[i])) !=
            std::tolower(static_cast<unsigned char>(cstr[i]))) {
            return false;
        }
    }

    return true;
}

template<typename Str>
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

template<typename Char>
auto lowercase(const Char* original) {
    std::basic_string<Char> str(original);

#ifndef WIN32
    std::transform(str.begin(), str.end(), str.begin(), (int(*)(int))std::tolower);
#else
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
#endif

    return str;
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
    // termFrequency stores the frequency of each term across the entire corpus.
    // For example, if the word "apple" appears 5 times in the entire corpus,
    // termFrequency["apple"] would be 5.
    std::unordered_map<std::string, int> termFrequency;
    
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
    bool empty() const { return termFrequency.empty(); }
    
    // Function to get a string representation of the structure
    std::string toStr() const;

    // Static function to return the class name as a string
    static std::string class_name() {
        return "PreprocessedData";
    }
};

PreprocessedData preprocess_corpus(const std::vector<std::string>& corpus);
std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus);
std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus, const PreprocessedData&);

namespace utils {

std::string read_file(const std::string& filename);
    
}

#endif
