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

namespace cmn::utils {
    // Explicitly instantiate the most commonly used forms of the contains function
    template bool contains<std::string, char>(const std::string& str, const char& needle) noexcept;
    template bool contains<std::string, std::string>(const std::string& str, const std::string& needle) noexcept;
    template bool contains<std::string_view, char>(const std::string_view& str, const char& needle) noexcept;
    template bool contains<std::string_view, std::string_view>(const std::string_view& str, const std::string_view& needle) noexcept;

    // find/replace
    template<typename T>
    auto _find_replace(const T& str, const T& oldStr, const T& newStr) -> std::conditional_t<std::is_same_v<T, std::string_view>, std::string, T>
    {
        using ReturnType = std::conditional_t<std::is_same_v<T, std::string_view>, std::string, T>;
        ReturnType result(str);
        
        size_t pos = 0;
        while((pos = result.find(oldStr, pos)) != T::npos)
        {
            result.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
        
        return result;
    }

    std::string find_replace(std::string_view str, std::string_view oldStr, std::string_view newStr)
    {
        return _find_replace<std::string_view>(str, oldStr, newStr);
    }
    std::wstring find_replace(const std::wstring& str, const std::wstring& oldStr, const std::wstring& newStr)
    {
        return _find_replace<std::wstring>(str, oldStr, newStr);
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
std::vector<std::basic_string_view<typename Str::value_type>>
_split_with_reserve(Str const& s, char c, bool skip_empty = false, bool trim = false)
{
    using CharT = typename Str::value_type;
    std::basic_string_view<CharT> sv(s);
    std::vector<std::basic_string_view<CharT>> ret;
    
    auto start = sv.begin();
    auto end = sv.end();
    while (start != end) {
        auto pos = std::find(start, end, c);
        auto len = pos - start;
        
        if (trim) {
            while (len > 0 and std::isspace(*(start + len - 1))) {
                len--;
            }
            while (len > 0 and std::isspace(*start)) {
                start++;
                len--;
            }
        }
        
        if (len > 0 or not skip_empty) {
            ret.emplace_back(&*start, len);
        }
        
        start = pos;
        if (start != end) {
            start++;
        }
    }
    
    if (not skip_empty and (end == sv.begin() or *(end - 1) == c)) {
        ret.emplace_back();
    }

    return ret;
}

    template <typename Str>
    std::vector<Str> _split(Str const& s, char c, bool skip_empty = false, bool trim = false) {
        std::vector<Str> ret;
        ret.reserve(2);
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
                ret.emplace_back(&*start, len);
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

    std::vector<std::string_view> split(std::string_view const& s, char c, bool skip_empty, bool trim) {
        return _split_with_reserve<std::string_view>(s, c, skip_empty, trim);
    }
    std::vector<std::string> split(std::string const& s, char c, bool skip_empty, bool trim) {
        return _split<std::string>(s, c, skip_empty, trim);
    }
    std::vector<std::basic_string_view<typename std::string::value_type>> split(std::string& s, char c, bool skip_empty, bool trim) {
        return _split_with_reserve<std::string>(s, c, skip_empty, trim);
    }

    std::vector<std::wstring> split(std::wstring const& s, char c, bool skip_empty, bool trim) {
        return _split<std::wstring>(s, c, skip_empty, trim);
    }
    std::vector<std::basic_string_view<typename std::wstring::value_type>> split(std::wstring& s, char c, bool skip_empty, bool trim) {
        return _split_with_reserve<std::wstring>(s, c, skip_empty, trim);
    }

    std::string read_file(const std::string& filename) {
        return cmn::file::Path(filename).read_file();
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

std::string PreprocessedData::toStr() const {
    std::string str = class_name();
    str += "<";
    str += "termFrequency=" + cmn::Meta::toStr(termFrequency) + " ";
    str += "docFrequency=" + cmn::Meta::toStr(docFrequency) + " ";
    str += "termVectors=" + cmn::Meta::toStr(termVectors) + " ";
    str += "termImportance=" + cmn::Meta::toStr(termImportance);
    str += ">";
    return str;
}

std::vector<std::string> split_words(const std::string& str) {
    std::vector<std::string> words;
    std::string word;
    for (char ch : str) {
        ch = std::tolower(ch);
        if (std::isalnum(ch) || (ch != ' ' && ch != '/' && ch != '\\' && ch != '_' && ch != '.' && ch != ',' && ch != ';' && ch != ':' && ch != '-')) {
            word.push_back(ch);
        } else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

std::vector<std::string> _split_words(std::string input) {
    // Assuming utils::lowercase is defined to convert the string to lowercase
    input = cmn::utils::lowercase(input);
    
    std::regex word_regex(R"([^\s/\\_\.,;:-]+)");
    std::sregex_token_iterator begin(input.begin(), input.end(), word_regex);
    std::sregex_token_iterator end;
    
    std::vector<std::string> words(begin, end);
    return words;
}

// Preprocess the corpus by creating a map of words to their corresponding document indices
// and a vector of repertoire vectors for each document
// Preprocess the corpus
PreprocessedData preprocess_corpus(const std::vector<std::string> &corpus) {
    PreprocessedData data;
    auto totalDocs = corpus.size();

    for (const auto &phrase : corpus) {
        std::unordered_map<std::string, int> localTermFreq;
        auto words = split_words(phrase);
        
        // Cache the tokenized document
        data.tokenizedCorpus.push_back(words);
        
        for (const auto &word : words) {
            ++localTermFreq[word];
        }

        for (const auto &entry : localTermFreq) {
            ++data.termFrequency[entry.first];
            ++data.docFrequency[entry.first];
        }
    }

    // Calculate termImportance
    for (const auto &termFreqEntry : data.termFrequency) {
        const std::string &term = termFreqEntry.first;
        double idf = std::log(static_cast<double>(totalDocs) / data.docFrequency[term]);
        data.termImportance[term] = idf;
    }

    // Create term vectors for each document in the corpus
    for (const auto &phrase : corpus) {
        std::unordered_map<std::string, double> termVector;
        auto words = split_words(phrase);
        for (const auto &word : words) {
            termVector[word] = data.termImportance[word]; // Use IDF as weight
        }
        data.termVectors.push_back(termVector);
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

    std::vector<size_t> prev_row(len2 + 1);
    std::vector<size_t> curr_row(len2 + 1);

    for (size_t j = 0; j <= len2; j++) {
        prev_row[j] = j;
    }

    for (size_t i = 1; i <= len1; i++) {
        curr_row[0] = i;
        for (size_t j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr_row[j] = cmn::min(prev_row[j]     + 1,
                                   curr_row[j - 1] + 1,
                                   prev_row[j - 1] + cost);
            // 8===D
        }
        prev_row.swap(curr_row);
    }

    return cmn::narrow_cast<int>(prev_row[len2]);
}

double cosine_similarity(const std::unordered_map<std::string, double>& a,
                         const std::unordered_map<std::string, double>& b) {
    double dotProduct = 0.0, normA = 0.0, normB = 0.0;
    for (const auto &term : a) {
        dotProduct += term.second * (b.count(term.first) ? b.at(term.first) : 0.0);
        normA += term.second * term.second;
    }
    for (const auto &term : b) {
        normB += term.second * term.second;
    }
    
    if(normA == 0 || normB == 0)
        return 0;
    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

std::vector<int> text_search(const std::string &search_text,
                             const std::vector<std::string> &corpus)
{
    return text_search(search_text, corpus, preprocess_corpus(corpus));
}

std::vector<int> text_search(const std::string &search_text,
                             const std::vector<std::string> &corpus,
                             const PreprocessedData& data) 
{
    std::vector<std::tuple<double, std::string, int>> relevanceScores;
    auto search_terms = split_words(search_text);
    
    // Build the term vector for the search query
    std::unordered_map<std::string, double> searchVector;
    for (const auto &term : search_terms) {
        searchVector[term] = data.termImportance.count(term) ? data.termImportance.at(term) : 1;
    }

    for (size_t i = 0; i < corpus.size(); ++i) {
        double relevance = cosine_similarity(searchVector, data.termVectors[i]);
        const auto& corpus_terms = data.tokenizedCorpus[i];
        
        double avgDistance = 0, samples = 0;
        
        for (const auto &search_term : search_terms) {
            double best_distance = INFINITY;
            for (const auto &corpus_term : corpus_terms) {
                double distance = levenshtein_distance(search_term, corpus_term)
                    / double(std::max(search_term.length(), corpus_term.length()));
                best_distance = std::min(best_distance, distance);
                
                if(best_distance == 0)
                    break; // cant get better than 0 distance i reckon
            }
            
            avgDistance += best_distance;
            ++samples;
        }
        
        avgDistance /= samples;
        relevance -= avgDistance;

        relevanceScores.push_back({relevance, corpus[i], i});
    }

    std::sort(relevanceScores.begin(), relevanceScores.end(), std::greater<>{});

    std::vector<int> sortedIndices;
    for (const auto &[relevance, str, index] : relevanceScores) {
        sortedIndices.push_back(index);
    }

    return sortedIndices;
}
