#include "UnresolvedStringPattern.h"
#include <gui/dyn/VarProps.h>
#include <misc/frame_t.h>
#include <gui/dyn/ParseText.h>
#include <misc/ranges.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>
#include <misc/default_settings.h>

namespace cmn::pattern {

template<typename Number>
    requires std::integral<Number>
bool is_valid_number(std::string_view str) {
    if(str.empty())
        return false;
    
    for(char c : str) {
        if(not std::isdigit(c))
            return false;
    }
    return true;
}

using namespace cmn::gui::dyn;
void handle_sub_objects(std::string& output, cmn::gui::dyn::VarBase_t& variable, const VarProps& modifiers);

UnresolvedStringPattern::UnresolvedStringPattern(UnresolvedStringPattern&& other)
    : original(std::move(other.original)),
      objects(std::move(other.objects)),
      typical_length(std::move(other.typical_length)),
      all_patterns(std::move(other.all_patterns))
{
    other.all_patterns.clear();
}

UnresolvedStringPattern::UnresolvedStringPattern(const UnresolvedStringPattern& other)
{
    *this = other;
}

UnresolvedStringPattern& UnresolvedStringPattern::operator=(UnresolvedStringPattern&& other) {
    for(auto pattern: all_patterns) {
        delete pattern;
    }
    original = std::move(other.original);
    objects = std::move(other.objects);
    typical_length = std::move(other.typical_length);
    all_patterns = std::move(other.all_patterns);
    
    other.all_patterns.clear();
    return *this;
}

UnresolvedStringPattern& UnresolvedStringPattern::operator=(const UnresolvedStringPattern& other)
{
    if (this == &other) return *this;

    // Delete old patterns
    for (auto pattern : all_patterns) {
        delete pattern;
    }
    all_patterns.clear();
    
    // this will contain the text for all the string_views
    if(other.original)
        original = std::make_unique<std::string>(*other.original);
    else
        original = nullptr;

    // Copy simple members
    objects = other.objects;
    typical_length = other.typical_length;

    const char* old_base = other.original ? other.original->data() : nullptr;
    const char* old_base_end = other.original ? other.original->data() + other.original->size() : nullptr;
    const char* new_base = original      ? original->data()      : nullptr;

    // Deep copy all_patterns and build a pointer mapping
    all_patterns.reserve(other.all_patterns.size());
    std::unordered_map<const Prepared*, Prepared*> prepared_map;
    for (const auto* pattern : other.all_patterns) {
        if (pattern) {
            Prepared* new_p = new Prepared(*pattern);
            all_patterns.push_back(new_p);
            prepared_map[pattern] = new_p;
        } else {
            all_patterns.push_back(nullptr);
        }
    }

    // Recursively fix all POINTER, PREPARED, and SV references in objects
    std::function<void(PreparedPattern&)> fix_ptrs;
    std::function<void(Prepared&)>        fix_prepared;   // non‑const; we mutate pointers

    fix_ptrs = [&](PreparedPattern& pat) {
        switch (pat.type) {
            case PreparedPattern::SV:
                // Remap the string_view so it points into this->original, not other's.
                if (old_base
                    && new_base
                    && pat.value.sv.data() >= old_base
                    && pat.value.sv.data() < old_base_end)
                {
                    std::ptrdiff_t offset = pat.value.sv.data() - old_base;
                    pat.value.sv = std::string_view(new_base + offset, pat.value.sv.size());
                }
                break;

            case PreparedPattern::POINTER:
                if (pat.value.ptr) {
                    auto it = prepared_map.find(pat.value.ptr);
                    assert(it != prepared_map.end());
                    if (it != prepared_map.end())
                    {             // pointer still old → remap
                        pat.value.ptr = it->second;
                    }
                    //fix_prepared(*pat.value.ptr);                // always recurse
                }
                break;

            case PreparedPattern::PREPARED:
                if (pat.value.prepared) {
                    auto it = prepared_map.find(pat.value.prepared);
                    assert(it != prepared_map.end());
                    if (it != prepared_map.end())
                    {              // still pointing at “other” ⇒ remap
                        pat.value.prepared = it->second;
                    }
                    fix_prepared(*pat.value.prepared);           // recurse
                }
                break;

            default:
                /* SV handled above; nothing to do for other enum values */
                break;
        }
    };

    fix_prepared = [&](Prepared& prep)
    {
        if (old_base
            && new_base
            && prep.original.data() >= old_base
            && prep.original.data() < old_base_end)
        {
            std::ptrdiff_t offset = prep.original.data() - old_base;
            prep.original = std::string_view(new_base + offset, prep.original.size());
        }
        
        for (auto& paramVec : prep.parameters)
            for (auto& child : paramVec)
                fix_ptrs(child);
        
        for(auto& sub : prep.subs) {
            for(auto &child: sub)
                fix_ptrs(child);
        }
    };

    for (auto& obj : objects)
        fix_ptrs(obj);

    return *this;
}

Unprepared create_parse_result(std::string_view trimmedStr) {
    if (not trimmedStr.empty() and trimmedStr.front() == '{' and trimmedStr.back() == '}')
    {
        trimmedStr = trimmedStr.substr(1, trimmedStr.size() - 2);
    }

    std::size_t pos = 0;
    
    Unprepared action;
    action.original = trimmedStr;
    
    while (pos < trimmedStr.size() and trimmedStr[pos] not_eq ':') {
        ++pos;
    }

    action.name = trimmedStr.substr(0, pos);
    bool inQuote = false, comment_out = false;
    char quoteChar = '\0';
    int braceLevel = 0;  // Keep track of nested braces
    std::size_t token_start = ++pos;  // Skip the first ':'

    for (; pos < trimmedStr.size(); ++pos) {
        char c = trimmedStr[pos];

        if(not comment_out) {
            if(c == '\\') {
                comment_out = true;
            } else if (c == ':' and not inQuote and braceLevel == 0) {
                action.parameters.emplace_back(trimmedStr.substr(token_start, pos - token_start));
                token_start = pos + 1;
            } else if ((c == '\'' or c == '\"') and (not inQuote or c == quoteChar)) {
                inQuote = not inQuote;
                if (inQuote) {
                    quoteChar = c;
                }
            } else if (c == '{') {
                if constexpr(not expand_expressions_in_quotes) {
                    if(inQuote)
                        continue;
                }
                // Assuming skipNested advances 'pos'
                gui::dyn::skipNested(trimmedStr, pos, '{', '}');
                --pos;
            } else if (c == '[' and not inQuote) {
                ++braceLevel;
            } else if (c == ']' and not inQuote) {
                --braceLevel;
            }
            
        } else {
            comment_out = false;
        }
    }

    if (inQuote) {
        throw std::invalid_argument("Invalid format: Missing closing quote");
    }
    
    if(comment_out) {
        throw std::invalid_argument("Invalid format: Ended on an escape character");
    }
    
    if (token_start < pos) {
        action.parameters.emplace_back(trimmedStr.substr(token_start, pos - token_start));
    }
    
    return action;
}

Unprepared extract_unprepared(std::string_view variable);

/// if `resolve_directly` is enabled, we essentially have the old "parse_text" behavior
/// where all expressions within the string are resolved immediately and the result is written to
/// an output string.
/// otherwise the expressions are parsed and saved within an UnpreparedPatterns object.
template<bool resolve_directly,
         typename Return_t>
Return_t parse_words(
    std::string_view pattern,
    [[maybe_unused]] std::conditional_t<resolve_directly, ResolveContext&&, bool> ctx)
{
    [[maybe_unused]] std::conditional_t<resolve_directly, bool, UnpreparedPatterns> result;
    std::stack<std::size_t> nesting_start_positions;
    bool comment_out = false;
    std::optional<std::size_t> in_string;
    std::optional<std::size_t> last_start;
    size_t collection_depth = 0;

    for(std::size_t i = 0; i < pattern.size(); ++i) {
        char ch = pattern[i];
        if (not is_in(ch, '\'', '"', '[', ']', '{', '}', ':', '\\')) {
            if(nesting_start_positions.empty()) {
                if(not last_start
                   && not comment_out
                   && ch != '{'
                   && ch != '\'')
                {
                    last_start = i;
                    
                }
            }
            
            if(comment_out)
                comment_out = false;
            continue;
        }
        
        if constexpr(expand_expressions_in_quotes) {
            if(ch == '\''
               && not nesting_start_positions.empty())
            {
                comment_out = false;
                continue;
            }
        }

        if(nesting_start_positions.empty()
           && not in_string)
        {
            if(ch == '['
               || (comment_out && ch == '{'))
            {
                ++collection_depth;
                
            } else if(collection_depth > 0
                      && (ch == ']' || (comment_out && ch == '}')))
            {
                --collection_depth;
            }
        }
        
        if(ch == '\'') {
            if(not comment_out) {
                bool keep_quotes = collection_depth > 0;
                
                if(in_string) {
                    if(not keep_quotes
                       && nesting_start_positions.empty())
                    {
                        auto current_word = pattern.substr(*in_string + 1, i - *in_string - 1);
                        if(current_word.find('\\') == std::string_view::npos) {
                            if constexpr(resolve_directly) {
                                ctx.output.append(current_word);
                            } else {
                                result.emplace_back(current_word);
                            }
                        } else {
                            if constexpr(resolve_directly) {
                                ctx.output.append(util::unescape(current_word));
                            } else {
                                result.emplace_back(util::unescape(current_word));
                            }
                        }
                        last_start = i + 1;
                    }
                    in_string.reset();
                    
                } else {
                    if(not keep_quotes
                       && nesting_start_positions.empty())
                    {
                        if(last_start && *last_start < i) {
                            std::string_view sv = pattern.substr(*last_start, i - *last_start);
                            if constexpr(resolve_directly) {
                                ctx.output.append(sv);
                            } else {
                                result.emplace_back(sv);
                            }
                        }
                        last_start.reset();
                    }
                    in_string = i;
                }
                
                continue;
                
            } else {
                comment_out = false;
            }
            
        } else if constexpr(expand_expressions_in_quotes) {
            const bool parse_quoted_brace = is_in(ch, '{', '}')
                || (ch == '\\'
                    && not comment_out
                    && i + 1 < pattern.size()
                    && is_in(pattern[i + 1], '{', '}'));
            
            if(in_string
               && nesting_start_positions.empty()
               && not parse_quoted_brace)
            {
                if(ch == '\\') {
                    comment_out = not comment_out;
                } else if(comment_out)
                    comment_out = false;
                continue;
            }
            
        } else if(in_string) {
            if(ch == '\\') {
                comment_out = not comment_out;
            } else if(comment_out)
                comment_out = false;
            continue;
        }
        
        if(nesting_start_positions.empty()) {
            if(not last_start
               && not comment_out
               && ch != '{'
               && ch != '\'')
            {
                last_start = i;
            }
            
            switch(ch) {
                    /*case '\\':
                    if(not comment_out) {
                        comment_out = true;
                        if(last_start && *last_start < i)
                            result.push_back(pattern.substr(*last_start, i - *last_start));
                        last_start = i + 1;
                        
                    } else {
                        comment_out = false;
                    }
                    break;
                case ':':
                    if(comment_out) {
                        comment_out = false;
                    }
                    break;*/
                case '{':
                    if(not comment_out){
                        nesting_start_positions.push(i + 1);
                        if(last_start) {
                            std::string_view sub = pattern.substr(*last_start, i - *last_start);
                            if constexpr(resolve_directly) {
                                ctx.output.append(sub);
                            } else {
                                result.push_back(sub);
                            }
                        }
                        last_start.reset();
                    }
                    comment_out = false;
                    break;
                case '}':
                    if(!comment_out)
                        throw InvalidSyntaxException("Mismatched closing brace at position ", i);
                    comment_out = false;
                    break;
                default:
                    if(comment_out) {
                        //FormatWarning("Invalid escape sequence at position ", i, ": ", ch, ". Only braces need to be escaped.");
                        comment_out = false;
                    }
            }
        } else {
            if(comment_out) {
                comment_out = false;
                
            } else if(ch == '\\') {
                comment_out = true;
                /*if(last_start && *last_start < i)
                     result.push_back(pattern.substr(*last_start, i - *last_start));
                last_start = i + 1;*/
                
            } else if(ch == '}') {
                if(nesting_start_positions.empty()) {
                    throw InvalidSyntaxException("Mismatched closing brace at position ", i);
                }
                
                std::size_t start_pos = nesting_start_positions.top();
                nesting_start_positions.pop();

                if(nesting_start_positions.empty()) {
                    std::string_view current_word = pattern.substr(start_pos, i - start_pos);
                    if(current_word.empty()) {
                        throw InvalidSyntaxException("Empty braces at position ", i);
                    }
                    
                    if constexpr(resolve_directly) {
                        //Print("Need to resolve: ", current_word);
                        
                        [[maybe_unused]] CTimer timer(current_word);
                        std::string resolved_word;
                        if(auto value = ctx.state.cached_variable_value(current_word, ctx.handler);
                           value.has_value())
                        {
                            resolved_word = value.value();
                            
                        } else {
                            resolved_word = resolve_variable(current_word, ctx.handler, ctx.context, ctx.state, [](const VarBase_t& variable, const VarProps& modifiers) -> std::string {
                                return _handle_subobjects(variable, modifiers);
                            }, [current_word](bool optional) -> std::string {
                                if(not optional)
                                    throw InvalidArgumentException("Cannot find variable or expression ", current_word,".");
                                //if(not optional)
                                //    FormatWarning("Cannot find variable or expression ", current_word,".");
                                //return optional ? "" : "null";
                                return "";
                            });
                            
                            ctx.state.set_cached_variable_value(ctx.handler, current_word, resolved_word);
                        }
                        ctx.output.append(resolved_word);
                        
                    } else {
                        result.push_back(extract_unprepared(current_word));
                    }
                    last_start = i + 1;
                }
                
            } else if(ch == '{') {
                nesting_start_positions.push(i + 1);
                if(last_start) {
                    std::string_view sv = pattern.substr(*last_start, i - *last_start);
                    if constexpr(resolve_directly) {
                        ctx.output.append(sv);
                    } else {
                        result.push_back(sv);
                    }
                }
                last_start.reset();
            } else {
                if(nesting_start_positions.empty()) {
                    throw InvalidSyntaxException("Mismatched opening brace at position ", i);
                }
                //nesting.top() += ch;
            }
        }
    }

    if(in_string)
        throw InvalidSyntaxException("Unterminated quoted string");

    if(not nesting_start_positions.empty()) {
        throw InvalidSyntaxException("Mismatched opening brace");
    }
    
    if(comment_out) {
        // Trailing backslash without a following character
        //throw InvalidSyntaxException("Trailing backslash");
    }

    if(last_start
       && *last_start < pattern.length())
    {
        std::string_view sv = pattern.substr(*last_start, pattern.length() - *last_start);
        if constexpr(resolve_directly) {
            ctx.output.append(sv);
        } else {
            result.push_back(sv);
        }
    }
    
    if constexpr(not resolve_directly) {
        return result;
    }
}

// Explicit instantiation definitions
template void parse_words<true>(std::string_view, [[maybe_unused]] ResolveContext&&);
template UnpreparedPatterns parse_words<false>(std::string_view, [[maybe_unused]] bool);

Unprepared extract_unprepared(std::string_view variable) {
    Unprepared props = create_parse_result(variable);
    
    bool html{false}, optional{false};
    std::size_t controlsSize = 0;
    for (char c : props.name) {
        if(c == '#' || c == '.') {
            if (c == '#') {
                html = true;
            }
            else if (c == '.') {
                optional = true;
            }
            controlsSize++;
        } else {
            // Once we hit a character that's not a control character,
            // we break out of the loop.
            break;
        }
    }

    // Parsing the name and sub-variables, omitting the controls
    auto r = utils::split(std::string_view(props.name).substr(controlsSize), '.');
    
    // Assigning the action name without the control characters.
    if (r.empty())
        throw InvalidArgumentException("No variables found: ", props.name);

    props.optional = optional;
    props.html = html;
    
    const size_t N = r.size();
    props.subs.resize(N - 1);
    for (size_t i=1, N = r.size(); i<N; ++i) {
        props.subs[i - 1] = parse_words<false>(r[i]);
        //props.subs[i - 1] = r[i];
    }

    if(r.front() != std::string_view(props.name))
        props.name = r.front();
    return props;
}

Prepared* Prepared::get(Unprepared& unprepared) {
    Prepared *prepared = new Prepared{
        .original = unprepared.original
    };
    
    prepared->parameters.reserve(unprepared.parameters.size());
    for(auto &o : unprepared.parameters) {
        assert(std::holds_alternative<UnpreparedPatterns>(o));
        PreparedPatterns r;
        for(auto &p : std::get<UnpreparedPatterns>(o)) {
            std::visit([&r](auto& obj) {
                using T = std::decay_t<decltype(obj)>;
                if constexpr(std::same_as<T, std::string_view>) {
                    r.push_back(PreparedPattern::make_sv(obj));
                } else if constexpr(std::same_as<T, std::string>) {
                    r.push_back(PreparedPattern::make_string(std::move(obj)));
                } else if constexpr(std::same_as<T, Unprepared>) {
                    r.push_back(PreparedPattern::make_prepared(Prepared::get(obj)));
                    
                } else {
                    static_assert(std::same_as<T, void>, "unknown type.");
                }
            }, p);
        }
        prepared->parameters.push_back(std::move(r));
    }
    
    prepared->subs.reserve(unprepared.subs.size());
    for(auto &o : unprepared.subs) {
        assert(std::holds_alternative<UnpreparedPatterns>(o));
        PreparedPatterns r;
        for(auto &p : std::get<UnpreparedPatterns>(o)) {
            std::visit([&r](auto& obj) {
                using T = std::decay_t<decltype(obj)>;
                if constexpr(std::same_as<T, std::string_view>) {
                    r.push_back(PreparedPattern::make_sv(obj));
                } else if constexpr(std::same_as<T, std::string>) {
                    r.push_back(PreparedPattern::make_string(std::move(obj)));
                } else if constexpr(std::same_as<T, Unprepared>) {
                    r.push_back(PreparedPattern::make_prepared(Prepared::get(obj)));
                    
                } else {
                    static_assert(std::same_as<T, void>, "unknown type.");
                }
            }, p);
        }
        prepared->subs.push_back(std::move(r));
    }
    
    auto& props = prepared->resolved;
    props.name = unprepared.name;
    props.html = unprepared.html;
    props.optional = unprepared.optional;
    return prepared;
}

std::string Prepared::toStr() const {
    std::string result = (std::string)resolved.name;
    if(not resolved.subs.empty()) {
        for(size_t j = 0; j < resolved.subs.size(); ++j) {
            result.append(".");
            result.append(resolved.subs[j]);
        }
    }
    result.append("{");
    for(size_t j = 0; j < parameters.size(); ++j) {
        auto &parm = parameters[j];
        if(j > 0)
            result += ",";
        if(parm.size() > 1)
            result += "(";
        
        for(size_t i = 0; i < parm.size(); ++i) {
            if(i > 0)
                result += ",";
            
            result += parm[i].toStr();
        }
        
        if(parm.size() > 1)
            result += ")";
    }
    result += "}";
    return result;
}

UnresolvedStringPattern::~UnresolvedStringPattern() {
    for(auto prepared : all_patterns)
        delete prepared;
}

void PreparedPattern::resolve(std::string& c, UnresolvedStringPattern& pattern, const std::shared_ptr<CurrentObjectHandler>& handler, const gui::dyn::Context& context, gui::dyn::State& state)
{
    auto is_cache_valid = [&handler](const Prepared& prepared) -> bool {
        if(not prepared.cached().has_value()
           || not prepared._cached_variable_versions.has_value())
        {
            return false;
        }
        
        //auto handler = state._current_object_handler.lock();
        if(not handler) {
            return true;
        }
        
        return prepared._cached_variable_versions->global == handler->variable_values_version()
            && prepared._cached_variable_versions->scoped == handler->scoped_variable_values_version();
    };
    
    switch(type) {
        case CHAR:
            c.push_back(value.ch);
            break;
        case STRING:
            c.append(*value.str);
            break;
        case SV:
            c.append(value.sv.data(), value.sv.size());
            break;
        case PREPARED:
            if(auto& o = *value.prepared;
               is_cache_valid(o))
            {
                auto& sv = o.cached().value();
                c.append(sv.data(), sv.size());
            } else {
                o.resolve(pattern, c, handler, context, state);
            }
            break;
        case POINTER:
            if(auto& o = *value.ptr;
               is_cache_valid(o))
            {
                auto& sv = o.cached().value();
                c.append(sv.data(), sv.size());
            } else {
                o.resolve(pattern, c, handler, context, state);
            }
            break;
        default:
#ifndef NDEBUG
            throw InvalidArgumentException("Illegal type ", type, ".");
#endif
            break;
    }
}

bool PreparedPattern::operator==(const PreparedPattern& other) const noexcept {
    if(type == other.type) {
        switch (type) {
            case CHAR:
                return value.ch == other.value.ch;
            case STRING:
                return *value.str == *other.value.str;
            case SV:
                return value.sv == other.value.sv;
            case PREPARED:
                return value.prepared == other.value.prepared;
            case POINTER:
                return value.ptr == other.value.ptr;
            default:
                break;
        }
    }
    return false;
}

bool PreparedPattern::operator<(const PreparedPattern& other) const noexcept {
    if(type != other.type)
        return type < other.type;
    switch (type) {
        case CHAR:
            return static_cast<unsigned char>(value.ch) < static_cast<unsigned char>(other.value.ch);
        case STRING:
            return *value.str < *other.value.str;
        case SV:
            return value.sv < other.value.sv;
        case PREPARED:
            return value.prepared->original < other.value.prepared->original;
        case POINTER:
            return value.ptr && other.value.ptr ? value.ptr->original < other.value.ptr->original : value.ptr < other.value.ptr;
            
        default:
            break;
    }
    return false;
}

std::string PreparedPattern::toStr() const {
    std::string str = "<";
    switch (type) {
        case CHAR:
            str.push_back(value.ch);
            break;
        case STRING:
            str.append(*value.str);
            break;
        case SV:
            str.append(value.sv);
            break;
        case PREPARED:
            str.append(value.prepared->toStr());
            break;
        case POINTER:
            str.append(hex(value.ptr).toStr());
            break;
            
        default:
            break;
    }
    str += ">";
    return str;
}

template<typename ApplyF, typename ErrorF>
    requires std::invocable<ApplyF, std::string&, VarBase_t&, const VarProps&>
inline auto resolve_variable(std::string& output, const VarProps& props, const std::shared_ptr<CurrentObjectHandler>& handler, const Context& context, State& state, ApplyF&& apply, ErrorF&& error)
{
    try {
        auto cached = props.subs.empty()
                        ? state.cached_variable_value(props.name, handler)
                        : state.cached_variable_value(utils::concat_views(props.name, props.subs), handler);
        if(cached)
        {
            output.append(cached.value());
            return;
            
        } else if(auto variable = context.find_variable(props.name, handler);
                  variable)
        {
            apply(output, *variable, props);
            return;
            
        } else if(props.name == "if") {
            REQUIRE_AT_LEAST(2, props);
            //[[maybe_unused]] CTimer ctimer("if");
            bool condition = props.parameters.front() == "true";
            //Print("* condition ", props.parameters.front()," => ", condition);
            if(condition)
                output.append(props.parameters[1]);
                //utils::fast_fromstr(output, props.parameters[1]);
            //output += Meta::fromStr<std::string>(props.parameters[1]);
            else if(props.parameters.size() == 3)
                output.append(props.parameters[2]);
                //utils::fast_fromstr(output, props.parameters[2]);
            //output += Meta::fromStr<std::string>(props.parameters[2]);
            return;
        } else if(props.name == "for") {
#ifndef NDEBUG
            Print("For loop: ", props);
#endif
            throw InvalidArgumentException("Not expecting a for loop here: ", props);
            return;
            
        } else if(auto it = context.defaults.variables.find(props.name); it != context.defaults.variables.end()) {
            //Print("* defaults contain ", props);
            //Print("* => ", it->second->value<std::string>(context, state));
            //[[maybe_unused]] CTimer ctimer("custom var");
            if(not props.subs.empty()) {
                /// here we might need to check for x,y / array accessors
                auto &key = props.subs.front();
                if(auto index = is_in_index(key, "x", "y", "w", "h");
                   index < 4u) {
                    /// treated special
                    std::string tmp = it->second->value<std::string>(context, state);
                    
                    auto parts = util::parse_array_parts(util::truncate(tmp));
                    if(index >= parts.size()) {
                        throw InvalidArgumentException("String 'array' ", tmp, " does not have ",index," elements and cant be accessed using .x/.y/.w/.h");
                    }
                    utils::fast_fromstr(output, parts[index]);
                    return;
                    
                } else if(is_valid_number<uint16_t>(key)) {
                    std::string tmp = it->second->value<std::string>(context, state);
                    auto parts = util::parse_array_parts(util::truncate(tmp));
                    
                    auto index = Meta::fromStr<uint8_t>(key);
                    utils::fast_fromstr(output, parts.at(index));
                    return;
                }
            }
            
            //utils::fast_fromstr(output, it->second->value<std::string>(context, state));
            output.append(it->second->value<std::string>(context, state));
            return;
            
        } else if(handler)
        {
            //Print("* ", props, " testing current handler");
            if(auto ptr = handler->retrieve_named(props.name);
               ptr != nullptr)
            {
                auto v = get_modifier_from_object(ptr.get(), props);
                if(v.has_value()) {
                    //Print("* ", props, " modifiers ", v.value());
                    utils::fast_fromstr(output, v.value());
                    //output += Meta::fromStr<std::string>(v.value());
                    return;
                }
            }
            
        }
        
    } catch(const std::exception& ex) {
        if constexpr(std::invocable<ErrorF, std::string&, bool, const std::string&>) {
            error(output, props.optional, std::string(ex.what()));
            return;
        }
    }
    
    if constexpr(std::invocable<ErrorF, std::string&, bool, const std::string&>) {
        error(output, props.optional, "Cannot find property "+props.toStr()+" in context: "+Meta::toStr(extract_keys(context.variables)));
        return;
    }
    else if constexpr(std::invocable<ErrorF, std::string&, bool>)
        error(output, props.optional);
    else
        error(output);
}


void resolve_parameter(std::string& c, UnresolvedStringPattern& pattern, PreparedPatterns& patterns, const std::shared_ptr<CurrentObjectHandler>& handler, const cmn::gui::dyn::Context& context, cmn::gui::dyn::State& state) {
    for(auto& o : patterns) {
        o.resolve(c, pattern, handler, context, state);
    }
}


void apply_html(std::string& output, const VarProps& modifiers, StringLike auto&& str) {
    if(modifiers.html)
        output += settings::htmlify(std::forward<decltype(str)>(str));
    else
        output += str;
};

template<typename String> void access_sub_field(std::string& output, const VarProps& modifiers, const glz::generic& json, std::span<String> subs);
template<typename String> void access_sub_field(std::string& output, const VarProps& modifiers, const Size2& json, std::span<String> subs);
template<typename String> void access_sub_field(std::string& output, const VarProps& modifiers, const Vec2& json, std::span<String> subs);

template<typename String>
void access_sub_field(std::string& output, const VarProps& modifiers, const Size2& size, std::span<String> subs)
{
    const auto& name = subs.front();
    //subs = subs.subspan(1); /// we are not continuing here anyway
    
    if(not is_in(name, "w", "h", "width", "height")) {
        if(not modifiers.optional)
            apply_html(output, modifiers, "null");
        return;
    }

    if(is_in(name, "w", "width"))
        apply_html(output, modifiers, Meta::toStr(size.width));
    else
        apply_html(output, modifiers, Meta::toStr(size.height));
}

template<typename String>
void access_sub_field(std::string& output, const VarProps& modifiers, const Vec2& pos, std::span<String> subs)
{
    const auto& name = subs.front();
    //subs = subs.subspan(1); /// we are not going further here anyway.
    
    if(not is_in(name, "x", "y")) {
        if(not modifiers.optional)
            apply_html(output, modifiers, "null");
        return;
    }

    if(is_in(name, "x"))
        apply_html(output, modifiers, Meta::toStr(pos.x));
    else
        apply_html(output, modifiers, Meta::toStr(pos.y));
}

template<typename Field, typename String>
void access_map_subfield(std::string& output, const VarProps& modifiers, std::span<String> subs, Field&& field) {
    if(field.is_array())  {
        if(not subs.empty()) {
            const auto& name = subs.front();
            if(is_valid_number<uint16_t>(name)) {
                auto idx = Meta::fromStr<uint16_t>(name);
                Print("* Accessing array index ", idx, " of ", field);
                subs = subs.subspan(1);
                
                auto ptr = field.access_array(idx);
                access_map_subfield(output, modifiers, subs, *ptr);
                return;
            } else {
                if(not modifiers.optional)
                    apply_html(output, modifiers, "null");
                return;
            }
        }
        
        /// just output the value then...

    } else if(field.template is_type<glz::generic>()) {
        // need another key
        if(not subs.empty()) {
            access_sub_field(output, modifiers, field.template value<glz::generic>(), subs);
            return;
        }
        
    } else if(field.template is_type<sprite::Map>()) {
        throw InvalidArgumentException("Not currently implemented to access a sprite::Map within a sprite::Map.");
    }
    
    apply_html(output, modifiers, Meta::fromStr<std::string>(field.valueString()));
}

template<typename String>
void access_sub_field(std::string& output, const VarProps& modifiers, const sprite::Map& map, std::span<String> subs)
{
    const auto& name = subs.front();
    subs = subs.subspan(1);
    
    if(not map.has(name)) {
        if(not modifiers.optional)
            apply_html(output, modifiers, "null");
        return;
        //throw InvalidArgumentException(map, " does not contain field ", name);
    }
    
    sprite::ConstReference field = map.at(name);
    if(field.valid())
        access_map_subfield(output, modifiers, std::move(subs), field.get());
    else if(not modifiers.optional)
        apply_html(output, modifiers, "null");
}

template<typename String>
void access_sub_field(std::string& output, const VarProps& modifiers, const glz::generic& json, std::span<String> subs)
{
    if(subs.empty()) {
        if(json.is_string()) {
            apply_html(output, modifiers, json.template get<std::string>());
            return;
        }
        
        apply_html(output, modifiers, Meta::toStr(json));
        return;
    }
    
    const auto& name = subs.front();
    subs = subs.subspan(1);
    
    if(json.is_array()) {
        const auto& array = json.get_array();
        if(size_t index = is_in_index(name, "x", "y", "w", "h");
           index < 4)
        {
            const auto &obj = array[index];
            if(obj.is_string()) {
                apply_html(output, modifiers, obj.get_string());
            } else
                apply_html(output, modifiers, Meta::toStr(obj));
            return;
        }
        else if(is_valid_number<uint16_t>(name)) {
            auto index = Meta::fromStr<uint16_t>(name);
            const auto &obj = array[index];
            if(obj.is_string()) {
                apply_html(output, modifiers, obj.get_string());
            } else
                apply_html(output, modifiers, Meta::toStr(obj));
            return;
        }
        
        throw InvalidArgumentException("Cannot access index ",name," in array ", json);
        
    } else if(json.is_object()) {
        auto &obj = json.get_object();
        auto it = obj.find(name);
        if(it == obj.end())
            throw InvalidArgumentException("Object ", json, " does not contain field ", name, " contains: ", extract_keys(obj));
        
        if(const glz::generic& field = it->second;
           not subs.empty())
        {
            access_sub_field(output, modifiers, field, subs);
            return;
        } else {
            if(field.is_string())
                apply_html(output, modifiers, field.get_string());
            else
                apply_html(output, modifiers, Meta::toStr(field));
            return;
        }
    }
    
    throw InvalidArgumentException("Unknown access type to ", json, " with ", subs);
}

void handle_sub_objects(std::string& output, cmn::gui::dyn::VarBase_t& variable, const VarProps& modifiers) {
    
    try {
        if(modifiers.subs.empty()) {
            apply_html(output, modifiers, variable.value_string(modifiers));
        } else if(variable.is<Size2>()) {
            variable.access_value<Size2>([&](auto&& value){
                std::span subs{ modifiers.subs };
                access_sub_field(output, modifiers, value, subs);
            }, modifiers);
            
        } else if(variable.is<Vec2>()) {
            variable.access_value<Vec2>([&](auto&& value){
                std::span subs{ modifiers.subs };
                access_sub_field(output, modifiers, value, subs);
            }, modifiers);
            
        } else if(variable.is<Range<Frame_t>>()) {
            if(modifiers.subs.front() == "start")
                apply_html(output, modifiers,
                           Meta::toStr(variable.value<Range<Frame_t>>(modifiers).start));
            else if(modifiers.subs.front() == "end")
                apply_html(output, modifiers,
                           Meta::toStr(variable.value<Range<Frame_t>>(modifiers).end));
            else
                throw InvalidArgumentException("Sub ",modifiers," of Range<Frame_t> is not valid.");
            
            
        } else if(variable.is<glz::generic>()) {
            variable.access_value<glz::generic>([&](auto&& value){
                std::span subs{ modifiers.subs };
                access_sub_field(output, modifiers, value, subs);
            }, modifiers);
        }
        else if(variable.is<sprite::Map>())
        {
            variable.access_value<sprite::Map>([&](auto&& value){
                std::span subs{ modifiers.subs };
                access_sub_field(output, modifiers, value, subs);
            }, modifiers);
            
        } else {
            if(not modifiers.subs.empty()) {
                /// need to check for special subs
                auto& key = modifiers.subs.front();
                if(auto index = is_in_index(key, "x", "y", "w", "h");
                   index < 4)
                {
                    std::string tmp = variable.value_string(modifiers);
                    auto parts = util::parse_array_parts(util::truncate(tmp));
                    if(index >= parts.size()) {
                        throw InvalidArgumentException("Expecting at least ",index," values in string 'array' ", tmp, " for index ", key," (=>",index,").");
                    }
                    
                    apply_html(output, modifiers, parts[index]);
                    return;
                    
                } else if(is_valid_number<uint16_t>(key)) {
                    auto index = Meta::fromStr<uint16_t>(key);
                    std::string tmp = variable.value_string(modifiers);
                    auto parts = util::parse_array_parts(util::truncate(tmp));
                    
                    apply_html(output, modifiers, parts.at(index));
                    //Print("* got unknown variable ", modifiers, " with subs => ", tmp);
                    return;
                } else {
                    /// here we trust the variable function to handle the modifiers
                    //throw InvalidArgumentException("Received subs ", modifiers.subs, " to variable of unknown/string type => ", variable.value_string(modifiers));
                }
            }
            apply_html(output, modifiers,
                       variable.value_string(modifiers));
        }
        
        //if(modifiers.html)
        //    return settings::htmlify(ret);
        return;
        
    } catch(const std::exception& ex) {
        if(not modifiers.optional)
            throw;//FormatExcept("Exception: ", ex.what(), " in variable: ", modifiers);
        output += modifiers.optional ? "" : "null";
    }
}

void Prepared::resolve(UnresolvedStringPattern& pattern, std::string& str, const std::shared_ptr<CurrentObjectHandler>& handler, const gui::dyn::Context& context, gui::dyn::State& state)
{
    auto& props = resolved;
    auto& parms = props.parameters;
    size_t index = str.length();
    
    auto update_cache = [&](){
        if(has_children) {
            _cached_value = std::string_view(str).substr(index);
            if(handler)
            {
                _cached_variable_versions = CachedVariableVersions{
                    .global = handler->variable_values_version(),
                    .scoped = handler->scoped_variable_values_version()
                };
            } else {
                _cached_variable_versions.reset();
            }
        }
    };
    
    props.subs.resize(subs.size());
    
    for(size_t i = 0; i < subs.size(); ++i) {
        props.subs[i].clear();
        for(auto &p : subs[i]) {
            p.resolve(props.subs[i], pattern, handler, context, state);
        }
    }
    
    if(resolved.name == "if") {
        /// we have a special case here where we want lazy evaluation
        /// lets assume three parameters here:
        if(not is_in(parameters.size(), 2u, 3u))
            throw InvalidArgumentException("An if statement (", parameters, ") accepts exactly 2 or 3 parameters.");
        
        parms.resize(parameters.size());
        auto& condition = parms[0];
        condition.clear();
        if(props.optional) {
            try {
                resolve_parameter(condition, pattern, parameters[0], handler, context, state);
            } catch(...) {
                FormatWarning("Failed to resolve optional if condition: ", props, " => ", parameters[0]);
                condition = "null";
            }
        } else {
            resolve_parameter(condition, pattern, parameters[0], handler, context, state);
        }
        
        if(gui::dyn::convert_to_bool(condition)) {
            parms[0] = "true";
            parms[1].clear();
            resolve_parameter(parms[1], pattern, parameters[1], handler, context, state);
            if(parms.size() == 3)
                parms[2] = "null";
        } else if(parms.size() == 3) {
            parms[0] = "false";
            parms[2].clear();
            resolve_parameter(parms[2], pattern, parameters[2], handler, context, state);
            parms[1] = "null";
        }
        
    } else if(resolved.name == "&&" || resolved.name == "||") {
        const bool is_and = resolved.name == "&&";
        //bool condition = is_and ? true : false;
        //parms.clear();
        //parms.reserve(parameters.size());
        std::string value;

        // Resolve operands in order so a decisive value protects the rest.
        for(auto& parameter : parameters) {
            value.clear();
            resolve_parameter(value, pattern, parameter, handler, context, state);
            const bool truth_value = gui::dyn::convert_to_bool(value);
            /*if(is_and)
                condition = condition && truth_value;
            else
                condition = condition || truth_value;*/
            
            if((is_and && not truth_value) || (not is_and && truth_value)) {
                str.append(truth_value ? "true" : "false");
                update_cache();
                return;
            }
        }
        
        if(is_and)
            str.append("true");
        else
            str.append("false");
        update_cache();
        return;

    } else if(resolved.name == "for") {
        if(not is_in(parameters.size(), 2u, 3u))
            throw InvalidArgumentException("An if statement (", parameters, ") only accepts 2 or 3 parameters.");
        
        size_t items_index = 0u;
        size_t fn_index = 1u;
        std::string variable_name = "i";
        
        if(parameters.size() == 3u) {
            items_index = 1u;
            fn_index = 2u;
            variable_name.clear();
            resolve_parameter(variable_name, pattern, parameters[0u], handler, context, state);
        }
        
        parms.resize(1u);
        parms[0].clear();
        resolve_parameter(parms[0], pattern, parameters[items_index], handler, context, state);
        
        bool is_string = false;
        if(not utils::beginsWith(parms[0], '[')
            || not utils::endsWith(parms[0], ']'))
        {
            /*if(not utils::beginsWith(parms[0], '\'')
               || not utils::endsWith(parms[0], '\''))
            {
                throw InvalidArgumentException("An if statement (", parameters, ") requires an array-like second parameter, received ", parms[0]);
            } else {*/
                /// fall back to string parsing
                is_string = true;
            //}
        }
        
        const size_t start_index = str.length();
        str.append("[");
        
        auto items = std::string_view(parms[0]);
        size_t idx = 0;
        if(is_string) {
            for(char c : items) {
                if(idx > 0)
                    str.append(",");
                
                auto scope = handler->scope();
                auto idx_str = Meta::toStr(idx);
                scope.set(variable_name, std::string(1, c));
                scope.set("index", idx_str);
                resolve_parameter(str, pattern, parameters[fn_index], handler, context, state);
                
                ++idx;
            }
        } else {
            for(auto item : util::parse_array_parts(util::truncate(items))) {
                if(idx > 0)
                    str.append(",");
                
                auto scope = handler->scope();
                auto idx_str = Meta::toStr(idx);
                scope.set(variable_name, item);
                scope.set("index", idx_str);
                resolve_parameter(str, pattern, parameters[fn_index], handler, context, state);
                
                ++idx;
            }
        }
        
        str.append("]");
        
        if(has_children) {
            _cached_value = std::string_view(str).substr(start_index);
            if(/*auto handler = state._current_object_handler.lock();*/
               handler)
            {
                _cached_variable_versions = CachedVariableVersions{
                    .global = handler->variable_values_version(),
                    .scoped = handler->scoped_variable_values_version()
                };
            } else {
                _cached_variable_versions.reset();
            }
        }
        
        return;
        
    } else {
        const size_t N = parameters.size();
        parms.resize(N);
        for(size_t i = 0; i < N; ++i) {
            auto &p = parameters[i];
            parms[i].clear();
            resolve_parameter(parms[i], pattern, p, handler, context, state);
        }
    }
    

    resolve_variable(str, props, handler, context, state, [](std::string& output, cmn::gui::dyn::VarBase_t& variable, const gui::dyn::VarProps& modifiers)
    {
        handle_sub_objects(output, variable, modifiers);
        
    }, [&props, &str](std::string&, bool optional, const std::string& ex = "") {
        if(not optional)
            throw InvalidArgumentException("Failed to evaluate ", props, ": ", no_quotes(not ex.empty() ? ex : std::string("<null>")));
        if(not props.optional)
            str.append("null");
    });
    
    update_cache();
}

std::string UnresolvedStringPattern::realize(const gui::dyn::Context& context, gui::dyn::State& state) {
    [[maybe_unused]] CTimer ctimer(*original);
    std::string str;
    bool has_length = typical_length.has_value();
    if(has_length)
        str.reserve(typical_length.value() * 1.5);
    
    for(auto ptr : all_patterns) {
        ptr->reset();
    }
    
    auto handler = state._current_object_handler.lock();
    if(not handler) {
        handler = std::make_shared<CurrentObjectHandler>();
        state._current_object_handler = std::weak_ptr(handler);
    }
    
    for(auto& object : objects) {
        object.resolve(str, *this, handler, context, state);
    }
    
    if(not has_length)
        typical_length = str.length();
    return str;
}

std::string UnresolvedStringPattern::toStr() const {
    // Produce a readable string representation of the pattern by
    // concatenating the textual form of each stored PreparedPattern.
    // We separate individual parts with a single space to match the
    // formatting style used elsewhere in the debug helpers.
    std::string result;
    result.reserve(objects.size() * 16);   // rough pre‑allocation

    bool first = true;
    for (const auto& obj : objects) {
        if (!first)
            result += ' ';
        result += obj.toStr();
        first = false;
    }
    return result;
}

UnresolvedStringPattern UnresolvedStringPattern::prepare_static(std::string_view _str) {
    auto result = UnresolvedStringPattern{};
    result.original = std::make_unique<std::string>(_str);
    std::string_view str{*result.original};
    
    result.objects.push_back(PreparedPattern::make_sv(str));
    
    return result;
}

UnresolvedStringPattern UnresolvedStringPattern::prepare(std::string_view _str) {
    auto result = UnresolvedStringPattern{};
    result.original = std::make_unique<std::string>(_str);
    std::string_view str{*result.original};
    
    auto unprepared = parse_words<false>(str);
    std::queue<Unprepared*> patterns;
    for(auto& r : unprepared) {
        std::visit([&](auto& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, Unprepared>) {
                patterns.push(&arg);
                
            } else if constexpr(std::same_as<T, std::string_view> || std::same_as<T, std::string>) {
                //result.objects.push_back(arg);
            } else {
                static_assert(std::same_as<T, void>, "Unknown Type.");
            }
            
        }, r);
    }
    
    while(not patterns.empty()) {
        auto unprep = patterns.front();
        patterns.pop();
        
        for(size_t i = 0; i < unprep->parameters.size(); ++i) {
            assert(std::holds_alternative<std::string_view>(unprep->parameters[i]));
            unprep->parameters[i] = parse_words<false>(std::get<std::string_view>(unprep->parameters[i]));
        }
        
        for(auto &v : unprep->parameters) {
            if(std::holds_alternative<std::string_view>(v))
                continue;
            
            for(auto &obj : std::get<UnpreparedPatterns>(v)) {
                std::visit([&patterns](auto& obj){
                    using T = std::decay_t<decltype(obj)>;
                    if constexpr(std::same_as<T, Unprepared>) {
                        patterns.push(&obj);
                    }
                }, obj);
            }
        }
    }
    
    for(auto &obj : unprepared) {
        std::visit([&result](auto& obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr(std::same_as<T, std::string_view>) {
                result.objects.push_back(PreparedPattern::make_sv(obj));
            } else if constexpr(std::same_as<T, std::string>) {
                result.objects.push_back(PreparedPattern::make_string(std::move(obj)));
            } else if constexpr(std::same_as<T, Unprepared>) {
                result.objects.push_back(PreparedPattern::make_prepared(Prepared::get(obj)));
                
            } else {
                static_assert(std::same_as<T, void>, "unknown type.");
            }
        }, obj);
    }
    
    /// second run, try to remove duplicates
    /// document current state:
    struct Preview {
        std::string_view name;
        bool operator<(const Preview& other) const {
            return name < other.name;
        }
    };
    
    std::deque<PreparedPattern*> q;
    std::map<Preview, size_t> counts;
    std::map<Preview, Prepared*> duplicates;
    
    for(auto& r : result.objects) {
        if(r.type == PreparedPattern::PREPARED)
            q.push_front(&r);
    }
    
    while(not q.empty()) {
        PreparedPattern* obj = q.front();
        q.pop_front();
        
        assert(obj->type == PreparedPattern::PREPARED);
        auto &prepared = obj->value.prepared;
        Preview preview{
            .name = prepared->original
        };
        counts[preview]++;
        
        auto it = duplicates.find(preview);
        if(it != duplicates.end()) {
            assert(not obj->value.prepared->has_children);
            delete obj->value.prepared;
            obj->value.ptr = (Prepared*)it->second;
            obj->type = PreparedPattern::POINTER;
            it->second->has_children = true;
            continue;
        }
        
        duplicates[preview] = prepared;
        result.all_patterns.push_back(prepared);
        
        for(auto &p : prepared->parameters) {
            for(auto &o : p) {
                if(o.type == PreparedPattern::PREPARED)
                    q.push_front(&o);
            }
        }

        for(auto &p : prepared->subs) {
            for(auto &o : p) {
                if(o.type == PreparedPattern::PREPARED)
                    q.push_front(&o);
            }
        }
    }
    
    /*{
        std::stringstream ss;
        ss << "[";
        
        for(auto& r : result.objects) {
            std::visit([&](auto&& obj){
                output_object(ss, std::forward<std::decay_t<decltype(obj)>>(obj));
            }, r);
        }
        
        ss << "]";
        Print(no_quotes(ss.str()));
    }*/
    
    /*for(auto &[key, value] : counts) {
        Print("* ", key.name, ": ", value);
    }*/
    
    return result;
}

}
