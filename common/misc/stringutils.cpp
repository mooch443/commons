#include <commons.pc.h>
#include "stringutils.h"
#include <regex>
#include <iomanip>
#include <algorithm>
#include <locale>
#include <functional>
#include <cstring>
#include <unordered_set>
#include <map>
#include <misc/format.h>
#include <file/Path.h>
#include <misc/checked_casts.h>

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

std::vector<std::string> split(const std::string &input) {
    std::regex word_regex(R"([^\s\W_]+)");
    std::sregex_token_iterator begin(input.begin(), input.end(), word_regex);
    std::sregex_token_iterator end;

    std::vector<std::string> words(begin, end);
    return words;
}

// Preprocess the corpus by creating a map of words to their corresponding document indices
// and a vector of repertoire vectors for each document
PreprocessedData preprocess_corpus(const std::vector<std::string>& corpus)
{
    PreprocessedData data;
    data.repertoire_vectors.resize(corpus.size());
    
    for (size_t i = 0; i < corpus.size(); ++i) {
        data.repertoire_vectors[i].resize(corpus[i].size());
        for (size_t j = 0; j < corpus[i].size(); j++) {
            data.repertoire_vectors[i][j] = corpus[i][j] - 'a';
        }
        
        std::vector<std::string> words = split(corpus[i]);
        for (const std::string& word : words) {
            data.word_to_doc_indices[word].insert(cmn::narrow_cast<int>(i));
        }
    }
    return data;
}

// Compute the Levenshtein distance between two strings
int levenshtein_distance(const std::string &s1, const std::string &s2) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();

    if (len1 < len2) {
        return levenshtein_distance(s2, s1);
    }

    std::vector<int> prev_row(len2 + 1);
    std::vector<int> curr_row(len2 + 1);

    for (size_t j = 0; j <= len2; j++) {
        prev_row[j] = j;
    }

    for (size_t i = 1; i <= len1; i++) {
        curr_row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr_row[j] = std::min({prev_row[j] + 1, curr_row[j - 1] + 1, prev_row[j - 1] + cost});
        }
        prev_row.swap(curr_row);
    }

    return prev_row[len2];
}

// Generate all possible substrings of length n from a given string
std::set<std::string> generate_substrings(const std::string &str, int n) {
    std::set<std::string> substrings;
    for (int i = 0; i + n <= str.length(); i++) {
        substrings.insert(str.substr(i, n));
    }
    return substrings;
}

// Computes the cosine similarity between two vectors
double cosine_similarity(const std::vector<double>& v1, const std::vector<double>& v2) {
    double dot_product = 0.0;
    double norm_v1 = 0.0;
    double norm_v2 = 0.0;
    bool zero_similarity = true;

    for (size_t i = 0; i < std::min(v2.size(), v1.size()); i++) {
        dot_product += v1[i] * v2[i];
        norm_v1 += v1[i] * v1[i];
        norm_v2 += v2[i] * v2[i];

        if (v1[i] != 0 && v2[i] != 0) {
            zero_similarity = false;
        }
    }

    if (zero_similarity) {
        return 0.0;
    }

    return dot_product / (sqrt(norm_v1) * sqrt(norm_v2));
}

// Compute the Jaccard similarity coefficient between two strings using substrings of length n
double jaccard_similarity(const std::string &str1, const std::string &str2, int n = 3) {
    std::set<std::string> substrings1 = generate_substrings(str1, n);
    std::set<std::string> substrings2 = generate_substrings(str2, n);

    std::set<std::string> intersection;
    std::set_intersection(substrings1.begin(), substrings1.end(), substrings2.begin(), substrings2.end(), std::inserter(intersection, intersection.begin()));

    std::set<std::string> uni;
    std::set_union(substrings1.begin(), substrings1.end(), substrings2.begin(), substrings2.end(), std::inserter(uni, uni.begin()));

    return static_cast<double>(intersection.size()) / static_cast<double>(uni.size());
}

std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus) {
    auto data = preprocess_corpus(corpus);
    return text_search(search_text, corpus, data);
}
std::vector<int> text_search(const std::string &search_text, const std::vector<std::string> &corpus, const PreprocessedData& data) {
    const int max_levenshtein_distance = 5;

    // Preprocess the search corpus
    const auto&[preprocessed_corpus, repertoire_distances] = data;//preprocess_corpus(corpus);

    // Split the search text into words
    std::vector<std::string> search_words = split(search_text);

    // Initialize the result set with the first word's results
    std::map<int, std::tuple<int, int, int>> index_distance_overlap_map;

    // Intersect the result vector with the subsequent words' results
    for (const auto& word : search_words) {
        std::vector<std::pair<int, int>> word_result_vector;

        for (const auto &entry : preprocessed_corpus) {
            int distance = levenshtein_distance(word, entry.first);
            if (distance <= max_levenshtein_distance || utils::contains(entry.first, word)) {
                for (int index : entry.second) {
                    word_result_vector.emplace_back(index, distance);
                }
            }
        }

        for (const auto& [index, distance] : word_result_vector) {
            auto &[sum, count, maximum] = index_distance_overlap_map[index];
            sum += distance;
            count++;
            maximum = std::max(maximum, distance);
        }
    }

    // Calculate the average distance and word overlap score
    std::vector<std::tuple<double, double, int>> result_vector;
    std::vector<std::tuple<double, double, std::string>> print_vector;
    for (const auto& [index, distance_overlap] : index_distance_overlap_map) {
        auto &[sum, count, maximum] = distance_overlap;
        double avg_distance = static_cast<double>(sum) / search_words.size();
        double overlap_score = count;
        
        const auto& repertoire_vector = repertoire_distances.at(index);
        std::vector<double> search_vector;
        for (char c : search_text) {
            search_vector.push_back(c - 'a');
        }
        
        overlap_score = cosine_similarity(repertoire_vector, search_vector);
        avg_distance = jaccard_similarity(corpus.at(index), search_text); //+ avg_distance / search_text.length());
        
        //print_vector.emplace_back(double(avg_distance), overlap_score, corpus.at(index));
        result_vector.emplace_back(double(avg_distance), overlap_score, index);
    }

    // Sort the result vector by relevance (ascending average levenshtein distance, descending word overlap score)
    std::sort(result_vector.begin(), result_vector.end(), std::greater<>{});
    //std::sort(print_vector.begin(), print_vector.end(), std::greater<>{});
    
    //cmn::print("print_vector for ", search_words, " is ", print_vector);

    // Convert the result vector to a vector of indexes
    std::vector<int> result_indexes;
    result_indexes.reserve(result_vector.size());
    for (const auto& [similarity, rank, index] : result_vector) {
        //if(similarity > 0)
            result_indexes.push_back(index);
    }

    return result_indexes;
}
