#include "ParseText.h"
#include <gui/dyn/State.h>
#include <gui/dyn/VarProps.h>
#include <gui/dyn/Action.h>
#include <gui/dyn/Context.h>
#include <gui/LabeledField.h>
#include <misc/default_settings.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/CTimer.h>

namespace cmn::gui::dyn {

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

template<typename T>
    requires (std::convertible_to<T, std::string_view> || std::same_as<T, Pattern>)
std::string _parse_text(const T& _pattern, const Context& context, State& state) {
    std::string_view pattern;
    if constexpr(std::same_as<T, Pattern>) {
        pattern = std::string_view(_pattern.original);
    } else
        pattern = std::string_view(_pattern);
    
    std::stringstream output;
    std::stack<std::size_t> nesting_start_positions;
    bool comment_out = false;

    for(std::size_t i = 0; i < pattern.size(); ++i) {
        char ch = pattern[i];
        if(nesting_start_positions.empty()) {
            switch(ch) {
                case '\\':
                    if(not comment_out) {
                        comment_out = true;
                    } else {
                        comment_out = false;
                        output << '\\';
                    }
                    break;
                case '{':
                    if(!comment_out) nesting_start_positions.push(i + 1);
                    else output << ch;
                    comment_out = false;
                    break;
                case '}':
                    if(!comment_out) throw InvalidSyntaxException("Mismatched closing brace at position ", i);
                    else output << ch;
                    comment_out = false;
                    break;
                default:
                    if(comment_out) {
                        FormatWarning("Invalid escape sequence at position ", i, ": ", ch, ". Only braces need to be escaped.");
                        comment_out = false;
                    }
                    output << ch;
            }
        } else {
            if(comment_out) {
                output << ch;
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
                    
                    [[maybe_unused]] CTimer timer(current_word);
                    std::string resolved_word;
                    if(auto value = state.cached_variable_value(current_word);
                       value.has_value())
                    {
                        resolved_word = value.value();
                        
                    } else {
                        resolved_word = resolve_variable(current_word, context, state, [](const VarBase_t& variable, const VarProps& modifiers) -> std::string {
                            try {
                                std::string ret;
                                if(modifiers.subs.empty())
                                    ret = variable.value_string(modifiers);
                                else if(variable.is<Size2>()) {
                                    if(modifiers.subs.front() == "w")
                                        ret = Meta::toStr(variable.value<Size2>(modifiers).width);
                                    else if(modifiers.subs.front() == "h")
                                        ret = Meta::toStr(variable.value<Size2>(modifiers).height);
                                    else
                                        throw InvalidArgumentException("Sub ",modifiers," of Size2 is not valid.");
                                    
                                } else if(variable.is<Vec2>()) {
                                    if(modifiers.subs.front() == "x")
                                        ret = Meta::toStr(variable.value<Vec2>(modifiers).x);
                                    else if(modifiers.subs.front() == "y")
                                        ret = Meta::toStr(variable.value<Vec2>(modifiers).y);
                                    else
                                        throw InvalidArgumentException("Sub ",modifiers," of Vec2 is not valid.");
                                    
                                } else if(variable.is<Range<Frame_t>>()) {
                                    if(modifiers.subs.front() == "start")
                                        ret = Meta::toStr(variable.value<Range<Frame_t>>(modifiers).start);
                                    else if(modifiers.subs.front() == "end")
                                        ret = Meta::toStr(variable.value<Range<Frame_t>>(modifiers).end);
                                    else
                                        throw InvalidArgumentException("Sub ",modifiers," of Range<Frame_t> is not valid.");
                                    
                                } else
                                    ret = variable.value_string(modifiers);
                                //throw InvalidArgumentException("Variable ", modifiers.name, " does not have arguments (requested ", modifiers.parameters,").");
                                //auto str = modifiers.toStr();
                                //Print(str.c_str(), " resolves to ", ret);
                                if(modifiers.html)
                                    return settings::htmlify(ret);
                                return ret;
                            } catch(const std::exception& ex) {
                                if(not modifiers.optional)
                                    FormatExcept("Exception: ", ex.what(), " in variable: ", modifiers);
                                return modifiers.optional ? "" : "null";
                            }
                        }, [](bool optional) -> std::string {
                            return optional ? "" : "null";
                        });
                        
                        state.set_cached_variable_value(current_word, resolved_word);
                    }
                    if(nesting_start_positions.empty()) {
                        output << resolved_word;
                    } else {
                        //nesting.top() += resolved_word;
                    }
                }
                
            } else if(ch == '{') {
                nesting_start_positions.push(i + 1);
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

    return output.str();
}

std::string parse_text(const std::string_view& pattern, const Context& context, State& state) {
    return _parse_text(pattern, context, state);
}
std::string parse_text(const Pattern& pattern, const Context& context, State& state) {
    return _parse_text(pattern, context, state);
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

    //Print("Initial parameters = ", props.parameters);
    //for(auto &p : props.parameters) {
        // parse parameters here
        // if the parameter seems to be a string (quotes '"), use parse_text(text, context) function to parse it
        // if the parameter seems to be a variable, it needs to be resolved:
    //    p = _parse_text(p, context, state);
    //}
    
    if(r.front() != std::string_view(props.name))
        props.name = r.front();
    return props;
}

}
