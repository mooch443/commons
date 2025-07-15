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

using namespace cmn::gui::dyn;

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

    fix_ptrs = [&](PreparedPattern& pat)
    {
        switch (pat.type)
        {
            case PreparedPattern::SV:
                // Remap the string_view so it points into this->original, not other's.
                if (old_base && new_base && pat.value.sv.data() >= old_base) {
                    std::ptrdiff_t offset = pat.value.sv.data() - old_base;
                    pat.value.sv = std::string_view(new_base + offset, pat.value.sv.size());
                }
                break;

            case PreparedPattern::POINTER:
                if (pat.value.ptr) {
                    auto it = prepared_map.find(pat.value.ptr);
                    if (it != prepared_map.end()) {             // pointer still old → remap
                        pat.value.ptr = it->second;
                    }
                    fix_prepared(*pat.value.ptr);                // always recurse
                }
                break;

            case PreparedPattern::PREPARED:
                if (pat.value.prepared) {
                    auto it = prepared_map.find(pat.value.prepared);
                    if (it != prepared_map.end()) {              // still pointing at “other” ⇒ remap
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
        for (auto& paramVec : prep.parameters)
            for (auto& child : paramVec)
                fix_ptrs(child);
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
            gui::dyn::skipNested(trimmedStr, pos, '{', '}');
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
        props.subs[i - 1] = r[i];
    }

    if(r.front() != std::string_view(props.name))
        props.name = r.front();
    return props;
}

UnpreparedPatterns parse_words(std::string_view pattern) {
    UnpreparedPatterns result;
    std::stack<std::size_t> nesting_start_positions;
    bool comment_out = false;
    std::optional<std::size_t> last_start;

    for(std::size_t i = 0; i < pattern.size(); ++i) {
        char ch = pattern[i];
        if(nesting_start_positions.empty()) {
            if(not last_start
               && not comment_out
               && ch != '{')
            {
                last_start = i;
            }
            
            switch(ch) {
                case '\\':
                    if(not comment_out) {
                        comment_out = true;
                    } else {
                        comment_out = false;
                    }
                    break;
                case '{':
                    if(not comment_out){
                        nesting_start_positions.push(i + 1);
                        if(last_start)
                            result.push_back(pattern.substr(*last_start, i - *last_start));
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
                        FormatWarning("Invalid escape sequence at position ", i, ": ", ch, ". Only braces need to be escaped.");
                        comment_out = false;
                    }
            }
        } else {
            if(comment_out) {
                comment_out = false;
                
            } else if(ch == '\\') {
                comment_out = true;
                
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
                    
                    result.push_back(extract_unprepared(current_word));
                    last_start = i + 1;
                }
                
            } else if(ch == '{') {
                nesting_start_positions.push(i + 1);
                if(last_start)
                    result.push_back(pattern.substr(*last_start, i - *last_start));
                last_start.reset();
            } else {
                if(nesting_start_positions.empty()) {
                    throw InvalidSyntaxException("Mismatched opening brace at position ", i);
                }
                //nesting.top() += ch;
            }
        }
    }

    if(not nesting_start_positions.empty()) {
        throw InvalidSyntaxException("Mismatched opening brace");
    }
    
    if(comment_out) {
        // Trailing backslash without a following character
        throw InvalidSyntaxException("Trailing backslash");
    }

    if(last_start
       && *last_start < pattern.length())
    {
        result.push_back(pattern.substr(*last_start, pattern.length() - *last_start));
    }
    
    return result;
}

Prepared* Prepared::get(const Unprepared& unprepared) {
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
                } else if constexpr(std::same_as<T, Unprepared>) {
                    r.push_back(PreparedPattern::make_prepared(Prepared::get(obj)));
                    
                } else {
                    static_assert(std::same_as<T, void>, "unknown type.");
                }
            }, p);
        }
        prepared->parameters.push_back(std::move(r));
    }
    
    auto& props = prepared->resolved;
    props.name = unprepared.name;
    props.html = unprepared.html;
    props.optional = unprepared.optional;
    for(auto &sv : unprepared.subs)
        props.subs.push_back((std::string)sv);
    return prepared;
}

std::string Prepared::toStr() const {
    std::string result = "fn<" + (std::string)resolved.name + ">{";
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

void PreparedPattern::resolve(std::string& c, UnresolvedStringPattern& pattern, const gui::dyn::Context& context, gui::dyn::State& state)
{
    switch(type) {
        case SV:
            c.append(value.sv.data(), value.sv.size());
            break;
        case PREPARED:
            if(auto& o = value.prepared;
               o->cached().has_value())
            {
                auto& sv = o->cached().value();
                c.append(sv.data(), sv.size());
            } else {
                o->resolve(pattern, c, context, state);
            }
            break;
        case POINTER:
            if(auto& o = *value.ptr;
               o.cached().has_value())
            {
                auto& sv = o.cached().value();
                c.append(sv.data(), sv.size());
            } else {
                o.resolve(pattern, c, context, state);
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
    std::string str = "PreparedPattern<";
    switch (type) {
        case SV:
            str += "sv<"+(std::string)value.sv+">";
            break;
        case PREPARED:
            str += "prepared<"+value.prepared->toStr()+">";
            break;
        case POINTER:
            str += "ptr<"+hex(value.ptr).toStr()+">";
            break;
            
        default:
            break;
    }
    str += ">";
    return str;
}

template<typename ApplyF, typename ErrorF>
    requires std::invocable<ApplyF, std::string&, VarBase_t&, const VarProps&>
inline auto resolve_variable(std::string& output, const VarProps& props, const Context& context, State& state, ApplyF&& apply, ErrorF&& error)
{
    try {
        if(auto it = context.find(props.name);
           it.has_value())
        {
            ///[[maybe_unused]] CTimer ctimer("normal");
            if constexpr(std::invocable<ApplyF, std::string&, VarBase_t&, const VarProps&>) {
                //Print("* applying ", props);
                apply(output, *it.value()->second, props);
                return;
            } else
                static_assert(std::invocable<ApplyF, std::string&, VarBase_t&, const VarProps&>);
            
        } else if(props.name == "if") {
            REQUIRE_AT_LEAST(2, props);
            //[[maybe_unused]] CTimer ctimer("if");
            bool condition = props.parameters.front() == "true";
            //bool condition = resolve_variable_type<bool>(p.parameters.at(0), context, state);
            //Print("* condition ", props.parameters.front()," => ", condition);
            if(condition)
                utils::fast_fromstr(output, props.parameters[1]);
                //output += Meta::fromStr<std::string>(props.parameters[1]);
            else if(props.parameters.size() == 3)
                utils::fast_fromstr(output, props.parameters[2]);
                //output += Meta::fromStr<std::string>(props.parameters[2]);
            return;
            
        } else if(auto it = context.defaults.variables.find(props.name); it != context.defaults.variables.end()) {
            //Print("* defaults contain ", props);
            //Print("* => ", it->second->value<std::string>(context, state));
            //[[maybe_unused]] CTimer ctimer("custom var");
            utils::fast_fromstr(output, it->second->value<std::string>(context, state));
            return;
            
        } else if(auto lock = state._current_object_handler.lock();
                  lock != nullptr)
        {
            //Print("* ", props, " testing current handler");
            if(auto ptr = lock->retrieve_named(props.name);
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
        error(output, props.optional, "Cannot find property "+props.toStr()+" in context.");
        return;
    }
    else if constexpr(std::invocable<ErrorF, std::string&, bool>)
        error(output, props.optional);
    else
        error(output);
}


void resolve_parameter(std::string& c, UnresolvedStringPattern& pattern, PreparedPatterns& patterns, const cmn::gui::dyn::Context& context, cmn::gui::dyn::State& state) {
    for(auto& o : patterns) {
        o.resolve(c, pattern, context, state);
    }
}

void Prepared::resolve(UnresolvedStringPattern& pattern, std::string& str, const gui::dyn::Context& context, gui::dyn::State& state)
{
    auto& props = resolved;
    auto& parms = props.parameters;
    
    if(resolved.name == "if") {
        /// we have a special case here where we want lazy evaluation
        /// lets assume three parameters here:
        if(not is_in(parameters.size(), 2u, 3u))
            throw InvalidArgumentException("An if statement (", parameters, ") accepts exactly 2 or 3 parameters.");
        
        parms.resize(parameters.size());
        auto& condition = parms[0];
        condition.clear();
        resolve_parameter(condition, pattern, parameters[0], context, state);
        
        if(gui::dyn::convert_to_bool(condition)) {
            parms[0] = "true";
            parms[1].clear();
            resolve_parameter(parms[1], pattern, parameters[1], context, state);
            if(parms.size() == 3)
                parms[2] = "null";
        } else if(parms.size() == 3) {
            parms[0] = "false";
            parms[2].clear();
            resolve_parameter(parms[2], pattern, parameters[2], context, state);
            parms[1] = "null";
        }
        
    } else {
        const size_t N = parameters.size();
        parms.resize(N);
        for(size_t i = 0; i < N; ++i) {
            auto &p = parameters[i];
            parms[i].clear();
            resolve_parameter(parms[i], pattern, p, context, state);
        }
    }
    
    size_t index = str.length();
    resolve_variable(str, props, context, state, [](std::string& output, cmn::gui::dyn::VarBase_t& variable, const gui::dyn::VarProps& modifiers)
    {
        auto apply_html = [&modifiers, &output](auto&& str) {
            if(modifiers.html)
                output += settings::htmlify(str);
            else
                output += str;
        };
        
        try {
            if(modifiers.subs.empty())
                apply_html(variable.value_string(modifiers));
            else if(variable.is<Size2>()) {
                if(modifiers.subs.front() == "w")
                    apply_html( Meta::toStr(variable.value<Size2>(modifiers).width));
                else if(modifiers.subs.front() == "h")
                    apply_html( Meta::toStr(variable.value<Size2>(modifiers).height));
                else
                    throw InvalidArgumentException("Sub ",modifiers," of Size2 is not valid.");
                
            } else if(variable.is<Vec2>()) {
                if(modifiers.subs.front() == "x")
                    apply_html(Meta::toStr(variable.value<Vec2>(modifiers).x));
                else if(modifiers.subs.front() == "y")
                    apply_html(Meta::toStr(variable.value<Vec2>(modifiers).y));
                else
                    throw InvalidArgumentException("Sub ",modifiers," of Vec2 is not valid.");
                
            } else if(variable.is<Range<Frame_t>>()) {
                if(modifiers.subs.front() == "start")
                    apply_html( Meta::toStr(variable.value<Range<Frame_t>>(modifiers).start));
                else if(modifiers.subs.front() == "end")
                    apply_html( Meta::toStr(variable.value<Range<Frame_t>>(modifiers).end));
                else
                    throw InvalidArgumentException("Sub ",modifiers," of Range<Frame_t> is not valid.");
                
            } else
                apply_html(variable.value_string(modifiers));
            
            //if(modifiers.html)
            //    return settings::htmlify(ret);
            return;
            
        } catch(const std::exception& ex) {
            if(not modifiers.optional)
                FormatExcept("Exception: ", ex.what(), " in variable: ", modifiers);
            output += modifiers.optional ? "" : "null";
        }
        
    }, [&props](std::string&, bool optional, const std::string& ex = "") {
        if(not optional)
            throw InvalidArgumentException("Failed to evaluate ", props, ": ", no_quotes(not ex.empty() ? ex : std::string("<null>")));
    });
    
    if(has_children)
        _cached_value = std::string_view(str).substr(index);
}

std::string UnresolvedStringPattern::realize(const gui::dyn::Context& context, gui::dyn::State& state) {
    std::string str;
    bool has_length = typical_length.has_value();
    if(has_length)
        str.reserve(typical_length.value() * 1.5);
    
    for(auto ptr : all_patterns) {
        ptr->reset();
    }
    
    for(auto& object : objects) {
        object.resolve(str, *this, context, state);
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

UnresolvedStringPattern UnresolvedStringPattern::prepare(std::string_view _str) {
    auto result = UnresolvedStringPattern{};
    result.original = std::make_unique<std::string>(_str);
    std::string_view str{*result.original};
    
    auto unprepared = parse_words(str);
    std::queue<Unprepared*> patterns;
    for(auto& r : unprepared) {
        std::visit([&](auto& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::same_as<T, Unprepared>) {
                patterns.push(&arg);
                
            } else if constexpr(std::same_as<T, std::string_view>) {
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
            unprep->parameters[i] = parse_words(std::get<std::string_view>(unprep->parameters[i]));
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
