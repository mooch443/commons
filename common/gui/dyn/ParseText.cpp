#include "ParseText.h"
#include <gui/dyn/State.h>
#include <gui/dyn/VarProps.h>
#include <gui/dyn/Action.h>
#include <gui/dyn/Context.h>
#include <gui/LabeledField.h>
#include <misc/default_settings.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/CTimer.h>
#include <gui/dyn/UnresolvedStringPattern.h>

namespace cmn::gui::dyn {

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

bool apply_modifier_to_object(std::string_view, const Layout::Ptr& object, const Action& value) {
    std::unordered_map<std::string_view, std::function<void(const Layout::Ptr&, const Action&)>, MultiStringHash, MultiStringEqual> applicators {
        { "pos",
            [](const Layout::Ptr& ptr, const Action& action) {
                Print("pos = ", action, " for ", ptr.get());
                LabeledField::delegate_to_proper_type(attr::Loc{Meta::fromStr<Vec2>(action.parameters.back())}, ptr);
            }
        },
        { "fill",
            [](const Layout::Ptr& ptr, const Action& action) {
                Print("fill = ", action, " for ", ptr.get());
                LabeledField::delegate_to_proper_type(attr::FillClr{Meta::fromStr<Color>(action.parameters.back())}, ptr);
            }
        }
    };
    
    if(auto it = applicators.find(value.parameters.front());
       it != applicators.end())
    {
        it->second(object, value);
        return true;
    }
    
    return false;
}

std::string access_sub_field(const glz::generic& json, std::queue<std::string_view> subs) {
    if(not subs.empty()) {
        const auto& name = subs.front();
        subs.pop();
        
        if(json.is_object()) {
            if(not json.contains(name))
                throw InvalidArgumentException("Object ", json, " does not contain field ", name);
            return access_sub_field(json[name], subs);
        
        } else if(json.is_array()) {
            auto index = Meta::fromStr<uint16_t>(name);
            auto& obj = json.get_array()[index];
            return access_sub_field(obj, subs);
        }
    }
    
    if(json.is_string())
        return json.template get<std::string>();
    return Meta::toStr(json);
}

std::string _handle_subobjects(const VarBase_t& variable, const VarProps& modifiers, std::optional<std::queue<std::string_view>> subs) {
    if(not subs) {
        subs = std::queue<std::string_view>{};
        for(auto &sub : modifiers.subs)
            subs->emplace(sub);
    }
    
    try {
        std::string ret;
        if(subs->empty()) {
            ret = variable.value_string(modifiers);
            
        } else if(variable.is<Size2>()) {
            if(subs->front() == "w")
                ret = Meta::toStr(variable.value<Size2>(modifiers).width);
            else if(subs->front() == "h")
                ret = Meta::toStr(variable.value<Size2>(modifiers).height);
            else
                throw InvalidArgumentException("Sub ",modifiers," of Size2 is not valid.");
            
        } else if(variable.is<Vec2>()) {
            if(subs->front() == "x")
                ret = Meta::toStr(variable.value<Vec2>(modifiers).x);
            else if(subs->front() == "y")
                ret = Meta::toStr(variable.value<Vec2>(modifiers).y);
            else
                throw InvalidArgumentException("Sub ",modifiers," of Vec2 is not valid.");
            
        } else if(variable.is<Range<Frame_t>>()) {
            if(subs->front() == "start")
                ret = Meta::toStr(variable.value<Range<Frame_t>>(modifiers).start);
            else if(subs->front() == "end")
                ret = Meta::toStr(variable.value<Range<Frame_t>>(modifiers).end);
            else
                throw InvalidArgumentException("Sub ",modifiers," of Range<Frame_t> is not valid.");
                
        } else if(variable.is<glz::generic>()) {
            return variable.access_value<glz::generic>([&](auto&& value){
                return access_sub_field(value, *subs);
            }, modifiers);
        }
        else if(variable.is<sprite::Map>()) {
            return variable.access_value<sprite::Map>([&](auto&& value) {
                if(auto key = subs->front();
                   value.has(key))
                {
                    subs->pop();
                    
                    auto prop = value.at(key);
                    assert(prop.valid());
                    if(prop->template is_type<glz::generic>()) {
                        return access_sub_field(prop->template value<glz::generic>(), *subs);
                    } else if(prop->is_array()
                              && not subs->empty())
                    {
                        auto p = prop->access_array(Meta::fromStr<uint16_t>(subs->front()));
                        if(p
                           && p->valid())
                        {
                            return Meta::fromStr<std::string>(p->valueString());
                        }
                        
                    } else if(prop->template is_type<sprite::Map>()) {
                        throw InvalidArgumentException("Accessing a sprite::Map inside a sprite::Map is unsupported (", modifiers,").");
                    }
                    
                    return Meta::fromStr<std::string>(value.at(key)->valueString());
                } else
                    throw InvalidArgumentException("No key named ", modifiers, " in map.");
            }, modifiers);
            
        } else {
            ret = variable.value_string(modifiers);
            //throw InvalidArgumentException("Variable ", modifiers.name, " does not have arguments (requested ", modifiers.parameters,").");
            //auto str = modifiers.toStr();
            //Print(str.c_str(), " resolves to ", ret);
            
            /// support array access for string-arrays
            if(subs
               && not subs->empty()
               && not ret.empty())
            {
                if(ret.front() == '['
                   && ret.back() == ']')
                {
                    if(is_in(subs->front(), "x", "y", "z")) {
                        /// we specifically allow x/y/z subtypes if the array has three elements
                        auto parts = util::parse_array_parts(util::truncate(ret));
                        if(parts.size() < 2)
                            throw InvalidArgumentException("Expected at least 2 array elements for string 'array' ", ret, ". Got ", parts, " instead.");
                        if(subs->front() == "x") {
                            ret = parts[0];
                        } else if(subs->front() == "y") {
                            ret = parts[1];
                        } else if(subs->front() == "z") {
                            if(parts.size() < 3)
                                throw InvalidArgumentException("Expected at least 3 array elements for string 'array' ", ret, ". Got ", parts, " instead.");
                            ret = parts[2];
                        }

                    } else if(is_valid_number<uint16_t>(subs->front())) {
                        /// must be resolved to an integer!
                        auto index = Meta::fromStr<uint16_t>(subs->front());
                        
                        /// this is an array
                        ret = util::parse_array_parts(util::truncate(ret)).at(index);
                    }
                    
                } else {
                    //throw InvalidArgumentException("Suspected string 'array' ", ret, " is not an array (with subs ", *subs,").");
                }
            }
        }
        
        if(modifiers.html)
            return settings::htmlify(ret);
        return ret;
    } catch(const std::exception& ex) {
        if(not modifiers.optional)
            throw;//FormatExcept("Exception: ", ex.what(), " in variable: ", modifiers);
        return modifiers.optional ? "" : "null";
    }
}

template<typename T>
    requires (std::convertible_to<T, std::string_view> || std::same_as<T, Pattern>)
std::string _parse_text(const T& _pattern, const std::shared_ptr<CurrentObjectHandler>& handler, const Context& context, State& state) {
    std::string_view pattern;
    if constexpr(std::same_as<T, Pattern>) {
        pattern = std::string_view(_pattern.original);
    } else
        pattern = std::string_view(_pattern);
    
    std::string _output;
    parse_words<true>(pattern, pattern::ResolveContext{
        .context = context,
        .state = state,
        .handler = handler,
        .output = _output
    });
    return _output;
    
    std::stringstream output;
    std::stack<std::size_t> nesting_start_positions;
    bool comment_out = false, in_string = false;
    size_t collection_depth = 0;

    for(std::size_t i = 0; i < pattern.size(); ++i) {
        char ch = pattern[i];
        
        bool parse_quoted_brace = false;
        if constexpr(expand_expressions_in_quotes) {
            parse_quoted_brace = is_in(ch, '{', '}')
                || (ch == '\\' 
                    && not comment_out 
                    && i + 1 < pattern.size()
                    && is_in(pattern[i + 1], '{', '}'));
            
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
                in_string = not in_string;
                
                if(collection_depth > 0
                   && nesting_start_positions.empty())
                {
                    output << ch;
                }
                
            } else {
                if(nesting_start_positions.empty()) {
                    output << ch;
                }
                comment_out = false;
            }
            continue;
            
        } else if(in_string
                  && nesting_start_positions.empty()
                  && (not expand_expressions_in_quotes
                      || not parse_quoted_brace))
        {
            if(ch == '\\') {
                comment_out = not comment_out;
            } else if(comment_out)
                comment_out = false;
            output << ch;
            continue;
            
        } else if constexpr(not expand_expressions_in_quotes) {
            if(in_string) {
                if(ch == '\\') {
                    comment_out = not comment_out;
                } else if(comment_out)
                    comment_out = false;
                continue;
            }
        }
        
        if(nesting_start_positions.empty()) {
            switch(ch) {
                case '\\':
                    if(not comment_out) {
                        comment_out = true;
                    } else {
                        comment_out = false;
                    }
                    output << '\\';
                    break;
                    /*case ':':
                    if(comment_out)
                        comment_out = false;
                    output << ch;
                    break;*/
                case '{':
                    if(not comment_out) {
                        nesting_start_positions.push(i + 1);
                    } else {
                        output << ch;
                        comment_out = false;
                    }
                    break;
                case '}':
                    if(not comment_out) {
                        throw InvalidSyntaxException("Mismatched closing brace at position ", i);
                    } else {
                        output << ch;
                        comment_out = false;
                    }
                    break;
                default:
                    if(comment_out) {
                        //FormatWarning("Invalid escape sequence at position ", i, ": ", std::string(1, ch), ". Only braces need to be escaped.");
                        comment_out = false;
                    }
                    output << ch;
            }
            
        } else {
            if(comment_out) {
                //output << ch;
                comment_out = false;
                
            } else if(ch == '\\') {
                comment_out = true;
                
            } else if(ch == '}') {
                std::size_t start_pos = nesting_start_positions.top();
                nesting_start_positions.pop();

                if(nesting_start_positions.empty()) {
                    std::string_view current_word = pattern.substr(start_pos, i - start_pos);
                    if(current_word.empty()) {
                        throw InvalidSyntaxException("Empty braces at position ", i);
                    }
                    
                    [[maybe_unused]] CTimer timer(current_word);
                    std::string resolved_word;
                    if(auto value = state.cached_variable_value(current_word, nullptr);
                       value.has_value())
                    {
                        resolved_word = value.value();
                        
                    } else {
                        resolved_word = resolve_variable(current_word, handler, context, state, [](const VarBase_t& variable, const VarProps& modifiers) -> std::string {
                            return _handle_subobjects(variable, modifiers);
                        }, [current_word](bool optional) -> std::string {
                            if(not optional)
                                throw InvalidSyntaxException("Cannot find variable or expression ", current_word,".");
                            //if(not optional)
                            //    FormatWarning("Cannot find variable or expression ", current_word,".");
                            //return optional ? "" : "null";
                            return "";
                        });
                        
                        state.set_cached_variable_value(handler, current_word, resolved_word);
                    }
                    if(nesting_start_positions.empty()) {
                        output << resolved_word;
                    } else {
                        //nesting.top() += resolved_word;
                    }
                }
                
            } else if(ch == '{') {
                nesting_start_positions.push(i + 1);
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

    return output.str();
}

std::string parse_text(const std::string_view& pattern, const Context& context, State& state) {
    return _parse_text(pattern, state._current_object_handler.lock(), context, state);
}

std::string parse_text(const std::string_view& pattern, const std::shared_ptr<CurrentObjectHandler>& handler, const Context& context, State& state) {
    return _parse_text(pattern, handler, context, state);
}

PreVarProps extractControls(const std::string_view& variable) {
    auto props = create_parse_result<PreVarProps>(variable);
    
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
    /*VarProps props{
        .name{std::string(r.front())},
        .optional = optional,
        .html = html
    };*/
    
    const size_t N = r.size();
    props.subs.resize(N - 1);
    for (size_t i=1, N = r.size(); i<N; ++i) {
        props.subs[i - 1] = r[i];
    }
    
    if(r.front() != std::string_view(props.name))
        props.name = r.front();
    return props;
}

}
