#include <commons.pc.h>
#include "stringutils.h"
#include <file/Path.h>

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
        if (std::isalnum(ch) || not cmn::is_in(ch, '*', '\t', '<', '>', '#', ' ', '/', '\\', '_', '.', ',', ';', ':', '-', '"', '`', '\'', '(', ')', '[', ']')) {
            word.push_back(ch);
            
        } else if (not word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

// Require a meaningful prefix length to avoid "t" matching everything.
static inline bool strong_prefix(const std::string& q, const std::string& t) {
    constexpr size_t kMin = 4;
    if (q.size() < kMin) return false;
    return t.size() >= q.size() && t.rfind(q, 0) == 0;
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
    data.tokenizedCorpus.reserve(totalDocs);

    // Split words (tokenize) and calculate in how many
    // documents each word appears
    for (const auto &phrase : corpus) {
        std::set<std::string_view> seen;
        auto words = split_words(phrase);
        
        for (const auto &word : words)
            seen.insert(word);

        for (auto entry : seen)
            ++data.docFrequency[(std::string)entry];
        
        // Cache the tokenized document
        data.tokenizedCorpus.push_back(std::move(words));
    }

    // Calculate termImportance
    for (const auto &termFreqEntry : data.docFrequency) {
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
        data.termVectors.push_back(std::move(termVector));
    }

    return data;
}

// Helper: build an IDF-weighted vector from tokens, using shared termImportance.
// Optional: apply a field-specific boost (e.g., namesBoost>1, docsBoost<1).
static std::unordered_map<std::string, double>
build_idf_vector(const std::vector<std::string>& tokens,
                 const std::unordered_map<std::string, double>& termImportance,
                 double fieldBoost = 1.0)
{
    std::unordered_map<std::string, int> tf;
    tf.reserve(tokens.size());

    for (const auto& t : tokens) ++tf[t];

    std::unordered_map<std::string, double> vec;
    vec.reserve(tf.size());

    for (const auto& [term, count] : tf) {
        // Better for docs: log-TF (still cheap, more informative):
        double w_idf = termImportance.at(term);
        double w_tf  = 1.0 + std::log(double(count));   // count>=1
        vec[term] = fieldBoost * (w_idf * w_tf);
    }
    return vec;
}

// Preprocess the corpus by creating a map of words to their corresponding document indices
// and a vector of repertoire vectors for each document
// Preprocess the corpus
PreprocessedDataWithDocs preprocess_corpus(const std::vector<std::string> &names, const std::vector<std::string> &docs)
{
    if(names.size() != docs.size()) {
        throw cmn::InvalidArgumentException("Corpus (", names.size(), ") and docs (", docs.size(),") have to have the same number of elements in the same order.");
    }
    
    PreprocessedDataWithDocs data;
    auto totalDocs = names.size();
    
    data.tokenizedNames.reserve(totalDocs);
    data.tokenizedDocs.reserve(totalDocs);

    // Split words (tokenize) and calculate in how many
    // documents each word appears
    for (size_t i = 0; i < totalDocs; ++i) {
        auto name_words = split_words(names[i]);
        auto doc_words  = split_words(docs[i]);
        
        // collect all words from everywhere (uniquely)
        std::set<std::string_view> seen;
        
        for (const auto &word : name_words)
            seen.insert(word);
        for (const auto &word : doc_words)
            seen.insert(word);

        // increase "how many documents does term appear in" counter per term
        for (auto entry : seen)
            ++data.docFrequency[(std::string)entry];
        
        // Cache the tokenized document
        data.tokenizedNames.push_back(std::move(name_words));
        data.tokenizedDocs.push_back(std::move(doc_words));
    }

    // Calculate termImportance
    for (const auto &termFreqEntry : data.docFrequency) {
        const std::string &term = termFreqEntry.first;
        const double df = termFreqEntry.second;
        
        // avoid mathematical edge cases
        double idf = std::log((double(totalDocs) + 1.0) / (double(df) + 1.0)) + 1.0;
        //double idf = std::log(static_cast<double>(totalDocs) / df);
        data.termImportance[term] = idf;
    }

    // Create term vectors for each document in the corpus
    data.nameVectors.reserve(data.tokenizedNames.size());
    data.docVectors.reserve(data.tokenizedDocs.size());
    
    constexpr double nameBoost = 2.5; // stronger signal
    constexpr double docBoost  = 1.0; // lower signal (you can also set <1)
    
    for (size_t i = 0; i < totalDocs; ++i) {
        data.nameVectors.push_back(build_idf_vector(data.tokenizedNames[i], data.termImportance, nameBoost));
        data.docVectors.push_back(build_idf_vector(data.tokenizedDocs[i], data.termImportance, docBoost));
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

std::vector<int> text_search(const std::string &search_text,
                             const std::vector<std::string> &names,
                             const std::vector<std::string>& docs,
                             const PreprocessedDataWithDocs& data)
{
    (void)docs;
    const size_t N = data.nameVectors.size();

    std::vector<std::tuple<double, std::string, int>> scores;
    scores.reserve(N);

    // Important: use the SAME normalization as preprocessing.
    auto search_terms = split_words(search_text);
    if (search_terms.empty()) {
        std::vector<int> idx(N);
        for (size_t i = 0; i < N; ++i) idx[i] = (int)i;
        return idx;
    }

    // Build query vector from normalized terms.
    std::unordered_map<std::string, double> searchVector;
    searchVector.reserve(search_terms.size());
    for (const auto &term : search_terms) {
        auto it = data.termImportance.find(term);
        searchVector[term] = (it != data.termImportance.end()) ? it->second : 1.0;
    }

    for (size_t i = 0; i < N; ++i) {
        const double name_relevance = cosine_similarity(searchVector, data.nameVectors[i]);
        const double doc_relevance  = cosine_similarity(searchVector, data.docVectors[i]);

        // Typo penalty: compare query terms to *name* tokens only (already normalized in preprocessing).
        const auto& name_terms = data.tokenizedNames[i];
        const auto& doc_terms = data.tokenizedDocs[i];

        double avgDistance = 0.0;
        for (const auto &q : search_terms) {
            double best = std::numeric_limits<double>::infinity();

            for (const auto &t : name_terms) {
                // Prefix match => perfect (fixes thresh -> threshold)
                if (strong_prefix(q, t) || strong_prefix(t, q)) { best = 0.0; break; }

                const int lev = levenshtein_distance(q, t);
                const double denom = double(std::max(q.size(), t.size()));
                const double d = denom > 0.0 ? double(lev) / denom : 1.0;
                if (d < best) best = d;
                if (best == 0.0) break;
            }

            if (!std::isfinite(best)) best = 1.0;
            avgDistance += best;
        }
        avgDistance /= double(search_terms.size());

        // Combine: name-first with a doc "rescue".
        // Docs mainly help when name match is weak.
        double score = name_relevance + 0.25 * (1.0 - name_relevance) * doc_relevance;

        // Crucial: if docs match (e.g. SAHI appears only in docs), don't let the
        // name-typo penalty swamp the score. Ramp penalty down aggressively with doc_relevance.
        // - doc_relevance >= ~0.34 => penalty ~0 (doc-only queries surface)
        // - doc_relevance == 0     => full penalty
        const double docGate = std::min(1.0, doc_relevance * 3.0);
        const double penaltyWeight = 0.8 * (1.0 - docGate);
        score -= penaltyWeight * avgDistance;

        scores.emplace_back(score, names[i], (int)i);
    }

    std::sort(scores.begin(), scores.end(),
              [&](const auto& a, const auto& b){
                  return std::get<0>(a) > std::get<0>(b);
              });

    std::vector<int> sortedIndices;
    sortedIndices.reserve(scores.size());
    for (const auto &[relevance, str, index] : scores) {
        (void)relevance; (void)str;
        sortedIndices.push_back(index);
    }

    return sortedIndices;
}

namespace cmn::utils {

// Production-ready function to strip HTML tags from a string.
template<cmn::StringLike Str>
std::string strip_html(const Str& input) {
    std::string_view sv(input);
    std::string output;
    enum class State { Outside, InsideTag, InsideQuote, InsideComment };
    State state = State::Outside;
    char quoteChar = '\0';

    size_t i = 0;
    while (i < sv.size()) {
        char c = sv[i];
        switch (state) {
            case State::Outside:
                if (c == '<') {
                    // Check if we are starting an HTML comment.
                    if (i + 3 < sv.size() && sv.substr(i, 4) == "<!--") {
                        state = State::InsideComment;
                        i += 4;
                    } else {
                        state = State::InsideTag;
                        ++i;
                    }
                } else {
                    output.push_back(c);
                    ++i;
                }
                break;

            case State::InsideTag:
                if (c == '"' || c == '\'') {
                    state = State::InsideQuote;
                    quoteChar = c;
                    ++i;
                } else if (c == '>') {
                    state = State::Outside;
                    ++i;
                } else {
                    ++i;
                }
                break;

            case State::InsideQuote:
                // Remain inside the quote until the matching quote is found.
                if (c == quoteChar)
                    state = State::InsideTag;
                ++i;
                break;

            case State::InsideComment:
                // Look for the end of the comment.
                if (i + 2 < sv.size() && sv.substr(i, 3) == "-->") {
                    state = State::Outside;
                    i += 3;
                } else {
                    ++i;
                }
                break;
        }
    }
    return output;
}
template std::string strip_html<std::string>(const std::string& input);
template std::string strip_html<std::string_view>(const std::string_view& input);
template std::string strip_html<const char*>(const char*const& input);

}
