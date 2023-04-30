#pragma once
#include <commons.pc.h>
#include <regex>

inline std::string unescape(const std::string &input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            if (input[i + 1] == '"' || input[i + 1] == '\'') {
                output.push_back(input[i + 1]);
                ++i;
            } else {
                output.push_back(input[i]);
            }
        } else {
            output.push_back(input[i]);
        }
    }

    return output;
}

inline std::map<std::string, std::string> parse_set_meta(const std::string &input) {
    std::map<std::string, std::string> result;
    std::regex regex_pattern(R"((\w+)\s*=\s*("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|[^;]+))");
    std::smatch matches;

    std::string::const_iterator search_start(input.cbegin());
    while (std::regex_search(search_start, input.cend(), matches, regex_pattern)) {
        std::string key = matches[1].str();
        std::string value = matches[2].str();

        // Remove quotes if present
        if ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
            value = unescape(value);
        }

        result[key] = value;
        search_start = matches.suffix().first;
    }

    return result;
}
