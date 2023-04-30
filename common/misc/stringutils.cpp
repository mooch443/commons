#include "stringutils.h"
#include <iomanip>
#include <algorithm>
#include <locale>
#include <functional>
#include <cstring>
#include <unordered_set>
#include <map>
#include <misc/format.h>
#include <file/Path.h>

namespace utils {
    /*
     * Begins with.
     */
    bool beginsWith(const std::string &str, const char needle) {
        return str.empty() ? false : (str.at(0) == needle);
    }
    bool beginsWith(const std::wstring &str, const wchar_t needle) {
        return str.empty() ? false : (str.at(0) == needle);
    }
    
    bool beginsWith(const std::string &str, const std::string &needle) {
        return str.compare(0, needle.length(), needle) == 0;
    }
    bool beginsWith(const std::wstring &str, const std::wstring &needle) {
        return str.compare(0, needle.length(), needle) == 0;
    }
    
    bool beginsWith(const std::string &str, const char *needle) {
        return str.compare(0, strlen(needle), needle) == 0;
    }
    
    /*
     * Ends with.
     */
    bool endsWith(const std::string &str, const char needle) {
        return str.empty() ? false : (str.back() == needle);
    }
    bool endsWith(const std::wstring &str, const wchar_t needle) {
        return str.empty() ? false : (str.back() == needle);
    }
    
    bool endsWith(const std::string &str, const std::string &needle) {
        if (needle.length() > str.length()) {
            return false;
        }
        return str.compare(str.length()-needle.length(), needle.length(), needle) == 0;
    }
    bool endsWith(const std::wstring &str, const std::wstring &needle) {
        if (needle.length() > str.length()) {
            return false;
        }
        return str.compare(str.length()-needle.length(), needle.length(), needle) == 0;
    }
    
    bool endsWith(const std::string &str, const char *needle) {
        const size_t len = strlen(needle);
        if (len > str.length()) {
            return false;
        }
        return str.compare(str.length()-len, len, needle) == 0;
    }

	bool contains(const std::string &str, const std::string &needle) {
		return str.find(needle) != std::string::npos;
	}
    
    bool contains(const std::string &str, const char &needle) {
        return str.find(needle) != std::string::npos;
    }

    bool contains(const std::wstring &str, const std::wstring &needle) {
        return str.find(needle) != std::wstring::npos;
    }

    bool contains(const std::wstring &str, const wchar_t &needle) {
        return str.find(needle) != std::wstring::npos;
    }

    // find/replace
    template<typename T>
    T _find_replace(const T& str, const T& oldStr, const T& newStr)
    {
        T result = str;
        
        size_t pos = 0;
        while((pos = result.find(oldStr, pos)) != T::npos)
        {
            result.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
        
        return result;
    }
    std::string find_replace(const std::string& str, const std::string& oldStr, const std::string& newStr)
    {
        return _find_replace<std::string>(str, oldStr, newStr);
    }
    std::wstring find_replace(const std::wstring& str, const std::wstring& oldStr, const std::wstring& newStr)
    {
        return _find_replace<std::wstring>(str, oldStr, newStr);
    }

    class Trie {
    public:
        void insert(const std::string& word, size_t index) {
            Trie* node = this;
            for (const auto& ch : word) {
                if (node->children.find(ch) == node->children.end()) {
                    node->children[ch] = std::make_unique<Trie>();
                }
                node = node->children[ch].get();
            }
            node->indices.insert(index);
        }

        Trie* get_child(char ch) {
            return children.find(ch) != children.end() ? children[ch].get() : nullptr;
        }

        const std::set<size_t>& get_indices() const {
            return indices;
        }

    private:
        std::map<char, std::unique_ptr<Trie>> children;
        std::set<size_t> indices;
    };

    std::string find_replace(const std::string& str, std::vector<std::tuple<std::string, std::string>> search_strings) {
        if (str.empty() || search_strings.empty()) {
            return str;
        }

        Trie trie;
        for (size_t i = 0; i < search_strings.size(); ++i) {
            trie.insert(std::get<0>(search_strings[i]), i);
        }

        std::string result;
        result.reserve(str.size());
        size_t i = 0;

        while (i < str.size()) {
            Trie* node = &trie;
            size_t j = i;
            std::set<size_t> matches;

            while (j < str.size() && (node = node->get_child(str[j]))) {
                matches.insert(node->get_indices().begin(), node->get_indices().end());
                ++j;
            }

            if (!matches.empty()) {
                size_t match = *matches.begin();
                result += std::get<1>(search_strings[match]);
                i += std::get<0>(search_strings[match]).size();
            } else {
                result += str[i++];
            }
        }

        return result;
    }

    std::string repeat(const std::string& s, size_t N) {
        std::string output;
        output.reserve(s.size() * N);
        for (size_t i = 0; i < N; ++i) {
            output += s;
        }
        return output;
    }

    template <typename Str>
    std::vector<Str> _split(Str const& s, char c, bool skip_empty = false, bool trim = false) {
        std::vector<Str> ret;
        std::basic_string_view<typename Str::value_type> sv(s);
        auto start = sv.begin();
        auto end = sv.end();
        while (start != end) {
            auto pos = std::find(start, end, c);
            auto len = pos - start;
            if (trim) {
                while (len > 0 && std::isspace(*(start + len - 1))) {
                    len--;
                }
                while (len > 0 && std::isspace(*start)) {
                    start++;
                    len--;
                }
            }
            if (len > 0 || !skip_empty) {
                ret.emplace_back(start, len);
            }
            start = pos;
            if (start != end) {
                start++;
            }
        }
        if (!skip_empty && (end == sv.begin() || *(end - 1) == c)) {
            ret.emplace_back();
        }
        return ret;
    }

    std::vector<std::string> split(std::string const& s, char c, bool skip_empty, bool trim) {
        return _split<std::string>(s, c, skip_empty, trim);
    }

    std::vector<std::wstring> split(std::wstring const& s, char c, bool skip_empty, bool trim) {
        return _split<std::wstring>(s, c, skip_empty, trim);
    }

    std::string read_file(const std::string& filename) {
        return file::Path(filename).read_file();
    }
}

static inline constexpr std::size_t InvalidMultibyteSequence = static_cast<std::size_t>(-1);
static inline constexpr std::size_t IncompleteMultibyteSequence = static_cast<std::size_t>(-2);

[[nodiscard]] std::wstring s2ws(const std::string& str)
{
    std::wstring result;
    std::mbstate_t state = std::mbstate_t();

    for (const auto& ch : str)
    {
        wchar_t wc;
        std::size_t res = std::mbrtowc(&wc, &ch, 1, &state);

        if (res == InvalidMultibyteSequence
            || res == IncompleteMultibyteSequence)
        {
            throw std::runtime_error("Invalid or incomplete multibyte sequence");
        }

        result += wc;
    }

    return result;
}

[[nodiscard]] std::string ws2s(const std::wstring& wstr)
{
    std::string result;
    std::mbstate_t state = std::mbstate_t();

    for (const auto& wc : wstr)
    {
        std::array<char, 4> mb;
        std::size_t res = std::wcrtomb(mb.data(), wc, &state);

        if (res == InvalidMultibyteSequence) {
            throw std::runtime_error("Invalid wide character");
        }

        result.append(mb.data(), res);
    }

    return result;
}

[[nodiscard]] std::wstring utf8ToWide(const std::u8string_view& utf8Str)
{
    std::wstring result;
    result.reserve(utf8Str.length());

    std::mbstate_t state = std::mbstate_t();
    const char8_t* in_next = utf8Str.data();
    const char8_t* in_end = utf8Str.data() + utf8Str.length();

    while (in_next < in_end)
    {
        wchar_t wc;
        std::size_t res = std::mbrtowc(&wc, reinterpret_cast<const char*>(in_next), in_end - in_next, &state);

        if (res == InvalidMultibyteSequence
            || res == IncompleteMultibyteSequence)
        {
            throw std::runtime_error("Invalid or incomplete multibyte sequence");
        }

        result.push_back(wc);
        in_next += res;
    }

    return result;
}

[[nodiscard]] std::u8string wideToUtf8(const std::wstring_view& wideStr)
{
    std::u8string result;
    result.reserve(wideStr.length() * 4);

    std::mbstate_t state = std::mbstate_t();
    for (const auto& wc : wideStr)
    {
        std::array<char, 4> mb;
        std::size_t res = std::wcrtomb(mb.data(), wc, &state);

        if (res == InvalidMultibyteSequence) {
            throw std::runtime_error("Invalid wide character");
        }


        result.append(reinterpret_cast<const char8_t*>(mb.data()), res);
    }

    return result;
}
