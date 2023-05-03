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

namespace utils {
    /**
     * Detects whether the given \p str begins with a given needle character.
     * @param str haystack
     * @param needle the needle
     * @return true if the given string starts with exactly the given needle
     */
    bool beginsWith(const std::string &str, const char needle);
    bool beginsWith(const std::wstring &str, const wchar_t needle);
    
    /**
     * Detects whether the given \p str begins with a given needle string.
     * @param str haystack
     * @param needle the needle
     * @return true if the given string starts with exactly the given needle
     */
    bool beginsWith(const std::string &str, const std::string &needle);
    bool beginsWith(const std::wstring &str, const std::wstring &needle);
    
    /**
     * Detects whether the given \p str begins with a given needle string.
     * The string \p needle has to be NULL terminated in order to work (this
     * method will use strlen() to detect the length).
     * @param str haystack
     * @param needle the needle
     * @return true if the given string starts with exactly the given needle
     */
    bool beginsWith(const std::string &str, const char *needle);
    
    /**
     * Detects whether the given \p str ends with a given needle character.
     * @param str haystack
     * @param needle the needle
     * @return true if the given string ends with exactly the given needle
     */
    bool endsWith(const std::string &str, const char needle);
    bool endsWith(const std::wstring &str, const wchar_t needle);
    
    /**
     * Detects whether the given \p str ends with a given needle string.
     * @param str haystack
     * @param needle the needle
     * @return true if the given string ends with exactly the given needle
     */
    bool endsWith(const std::string &str, const std::string &needle);
    bool endsWith(const std::wstring &str, const std::wstring &needle);
    
    /**
     * Detects whether the given \p str ends with a given needle string.
     * The string \p needle has to be NULL terminated in order to work (this
     * method will use strlen() to detect the length).
     * @param str haystack
     * @param needle the needle
     * @return true if the given string ends with exactly the given needle
     */
    bool endsWith(const std::string &str, const char *needle);

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
    
    // trim from start
    template<typename Str>
    Str &ltrim(Str &s) {
        const static std::function<bool(int)> pred = [](int c) -> bool {return !std::isspace(c);};
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), pred));
        return s;
    }

    // trim from end
    template<typename Str>
    Str &rtrim(Str &s) {
        const static std::function<bool(int)> pred = [](int c) -> bool  {return !std::isspace(c);};
        s.erase(std::find_if(s.rbegin(), s.rend(), pred).base(), s.end());
        return s;
    }

    // trim from start
    template<typename Str>
    Str ltrim(const Str &str) {
        Str s(str);
        const static std::function<bool(int)> pred = [](int c) -> bool  {return !std::isspace(c);};
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), (pred)));
        return s;
    }

    // trim from end
    template<typename Str>
    Str rtrim(const Str &str) {
        Str s(str);
        const static std::function<bool(int)> pred = [](int c) -> bool  {return !std::isspace(c);};
        s.erase(std::find_if(s.rbegin(), s.rend(), pred).base(), s.end());
        return s;
    }

    // trim from both ends
    template<typename Str>
    Str trim(Str const& s) {
        Str tmp = s;
        ltrim(rtrim(tmp));
        
        return tmp;
    }
    

// to lower case
template<typename Str>
Str lowercase(Str const& original) {
    Str s = original;
#ifndef WIN32
    std::transform(s.begin(), s.end(), s.begin(), (int(*)(int))std::tolower);
#else
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
#endif
    return s;
}

inline std::string lowercase(const char* original) {
    return lowercase(std::string(original));
}

// to upper case
template<typename Str>
Str uppercase(Str const& original) {
    Str s = original;
#ifndef WIN32
    std::transform(s.begin(), s.end(), s.begin(), (int(*)(int))std::toupper);
#else
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
#endif
    return s;
}

inline std::string uppercase(const char* original) {
    return uppercase(std::string(original));
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
std::vector<std::string> split(std::string const& s, char c, bool skip_empty = false, bool trim = false);

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

std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus);

namespace utils {

std::string read_file(const std::string& filename);
    
}

#endif
