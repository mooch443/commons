#pragma once

#include <commons.pc.h>
#include <misc/derived_ptr.h>
#include <gui/dyn/UnresolvedStringPattern.h>

namespace cmn::gui {
class Drawable;
}

namespace cmn::pattern {
struct UnresolvedStringPattern;
}

namespace cmn::gui::dyn {

struct Context;
struct State;
struct PreVarProps;
//struct Pattern;
using Pattern = pattern::UnresolvedStringPattern;
struct Action;

std::string parse_text(const std::string_view& pattern, const Context&, State&);

//std::string parse_text(const Pattern& pattern, const Context&, State&);

bool apply_modifier_to_object(std::string_view name, const derived_ptr<Drawable>& object, const Action& value);

// Function to skip over nested structures so they don't interfere with our parsing
auto skipNested(const auto& trimmedStr, std::size_t& pos, char openChar, char closeChar) {
    int balance = 1;
    pos++; // Skip the opening char
    while (pos < trimmedStr.size() and balance > 0) {
        if (trimmedStr[pos] == openChar) {
            balance++;
        } else if (trimmedStr[pos] == closeChar) {
            balance--;
        }
        pos++;
    }
    //return trimmedStr.substr(startPos, pos - startPos);  // Return the nested structure as a string
};

template<typename R>
R create_parse_result(std::string_view trimmedStr) {
    if (not trimmedStr.empty() and trimmedStr.front() == '{' and trimmedStr.back() == '}') {
        if constexpr(std::same_as<R, PreVarProps>) {
            trimmedStr = trimmedStr.substr(1, trimmedStr.size() - 2);
        } else {
            R action;
            action.original = std::string(trimmedStr);
            action.name = std::string_view(action.original);
            return action;
        }
    }

    std::size_t pos = 0;
    
    R action;
    action.original = std::string(trimmedStr);
    trimmedStr = std::string_view(action.original);
    
    while (pos < trimmedStr.size() and trimmedStr[pos] not_eq ':') {
        ++pos;
    }

    action.name = trimmedStr.substr(0, pos);
    bool inQuote = false;
    char quoteChar = '\0';
    int braceLevel = 0;  // Keep track of nested braces
    std::size_t token_start = ++pos;  // Skip the first ':'

    for (; pos < trimmedStr.size(); ++pos) {
        char c = trimmedStr[pos];

        if (c == ':' and not inQuote and braceLevel == 0) {
            action.parameters.emplace_back(trimmedStr.substr(token_start, pos - token_start));
            token_start = pos + 1;
        } else if ((c == '\'' or c == '\"') and (not inQuote or c == quoteChar)) {
            inQuote = not inQuote;
            if (inQuote) {
                quoteChar = c;
            }
        } else if (c == '{' and not inQuote) {
            // Assuming skipNested advances 'pos'
            skipNested(trimmedStr, pos, '{', '}');
            --pos;
        } else if (c == '[' and not inQuote) {
            ++braceLevel;
        } else if (c == ']' and not inQuote) {
            --braceLevel;
        }
    }

    if (inQuote) {
        throw std::invalid_argument("Invalid format: Missing closing quote");
    }
    
    if (token_start < pos) {
        action.parameters.emplace_back(trimmedStr.substr(token_start, pos - token_start));
    }
    
    return action;
}

PreVarProps extractControls(const std::string_view& variable);

template<utils::StringLike Str>
bool convert_to_bool(Str&& p) noexcept {
    if (   p == "false"
        || p == "[]"
        || p == "{}"
        || p == "0"
        || p == "null"
        || p == "''"
        || p == "\"\""
        || p == "0.0"
        || p == ""
        || p == "Map<size:0>"
        )
      return false;

    return true;
}

template<typename ValueType, typename PatternMapType>
inline std::optional<ValueType> parse_value(std::string_view name, PatternMapType& patterns, const Context& context, State& state)
{
    typename PatternMapType::iterator it = patterns.find(name);
    if(it == patterns.end())
        return std::nullopt;
    
    Pattern &pattern = it->second;
    if constexpr(std::same_as<ValueType, std::string>) {
        return pattern.realize(context, state);
        //return parse_text(pattern, context, state);
    } else {
        return Meta::fromStr<ValueType>(pattern.realize(context, state));
        //return Meta::fromStr<ValueType>(parse_text(pattern, context, state));
    }
};

template<typename ValueType, typename PatternMapType, typename CleanVT = remove_cvref_t<ValueType>>
inline CleanVT parse_value_with_default(ValueType&& default_value, std::string_view name, PatternMapType& patterns, const Context& context, State& state)
{
    auto value = parse_value<CleanVT>(name, patterns, context, state);
    if(not value.has_value())
        return default_value;
    return std::move(value.value());
};

}
