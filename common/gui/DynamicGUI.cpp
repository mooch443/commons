#include "DynamicGUI.h"
#include <gui/types/Button.h>
#include <gui/types/StaticText.h>
#include <misc/SpriteMap.h>
#include <file/DataLocation.h>
#include <gui/types/Textfield.h>
#include <gui/types/Checkbox.h>
#include <gui/types/Dropdown.h>
#include <misc/GlobalSettings.h>
#include <gui/types/SettingsTooltip.h>
#include <common/misc/default_settings.h>
#include <gui/ParseLayoutTypes.h>
#include <gui/types/ErrorElement.h>
#include <gui/DrawBase.h>
#include <regex>
#include <stack>

namespace gui {
namespace dyn {

// Function to skip over nested structures so they don't interfere with our parsing
auto skipNested(const auto& trimmedStr, std::size_t& pos, char openChar, char closeChar) {
    int balance = 1;
    std::size_t startPos = pos;
    pos++; // Skip the opening char
    while (pos < trimmedStr.size() and balance > 0) {
        if (trimmedStr[pos] == openChar) {
            balance++;
        } else if (trimmedStr[pos] == closeChar) {
            balance--;
        }
        pos++;
    }
    return trimmedStr.substr(startPos, pos - startPos);  // Return the nested structure as a string
};

Action Action::fromStr(std::string_view str) {
    auto trimmedStr = str;

    if (trimmedStr.front() == '{' and trimmedStr.back() == '}') {
        trimmedStr = trimmedStr.substr(1, trimmedStr.size() - 2);
    }

    Action action;
    std::size_t pos = 0;
    
    while (pos < trimmedStr.size() and trimmedStr[pos] not_eq ':') {
        ++pos;
    }

    action.name = trimmedStr.substr(0, pos);
    std::string token;
    bool inQuote = false;
    char quoteChar = '\0';
    int braceLevel = 0;  // Keep track of nested braces
    ++pos;  // Skip the first ':'

    for (; pos < trimmedStr.size(); ++pos) {
        char c = trimmedStr[pos];

        if (c == ':' and not inQuote and braceLevel == 0) {
            action.parameters.push_back(token);
            token.clear();
        } else if ((c == '\'' or c == '\"') and (not inQuote or c == quoteChar)) {
            inQuote = not inQuote;
            token += c;
            if (inQuote) {
                quoteChar = c;
            }
        } else if (c == '{' and not inQuote) {
            token += skipNested(trimmedStr, pos, '{', '}');
            --pos;
        } else if (c == '[' and not inQuote) {
            ++braceLevel;
            token += c;
        } else if (c == ']' and not inQuote) {
            --braceLevel;
            token += c;
        } else {
            token += c;
        }
    }

    if (inQuote) {
        throw std::invalid_argument("Invalid format: Missing closing quote");
    }
    
    if (not token.empty()) {
        action.parameters.push_back(token);
    }
    
    return action;
}

std::string Action::toStr() const {
    return "Action<"+name+" parms="+Meta::toStr(parameters)+">";
}

std::string VarProps::toStr() const {
    return "VarProps<"+std::string(name)+" parm="+Meta::toStr(parameters)+" subs="+Meta::toStr(subs)+">";
}

VarProps extractControls(const std::string_view& variable, const Context& context) {
    auto action = Action::fromStr(variable);
    
    bool html{false}, optional{false};
    std::size_t controlsSize = 0;
    for (char c : action.name) {
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
    auto r = utils::split(std::string_view(action.name).substr(controlsSize), '.');
    
    // Assigning the action name without the control characters.
    if (r.empty())
        throw InvalidArgumentException("No variables found: ", action.name);

    VarProps props{
        .name{std::string(r.front())},
        .optional = optional,
        .html = html
    };
    
    const size_t N = r.size();
    props.subs.resize(N - 1);
    for (size_t i=1, N = r.size(); i<N; ++i) {
        props.subs[i - 1] = r[i];
    }

    props.parameters = std::move(action.parameters);
    //print("Initial parameters = ", props.parameters);
    for(auto &p : props.parameters) {
        // parse parameters here
        // if the parameter seems to be a string (quotes '"), use parse_text(text, context) function to parse it
        // if the parameter seems to be a variable, it needs to be resolved:
        p = parse_text(p, context);
    }
    
    return props;
}

bool Context::has(const std::string_view & name) const noexcept {
    if(variables.contains(name))
        return true;
    
    init();
    return system_variables.contains(name);
}

template<typename T>
T map_vectors(const VarProps& props, auto&& apply) {
    if(props.parameters.size() != 2) {
        throw InvalidArgumentException("Invalid number of variables for vectorAdd: ", props);
    }
    
    T A{}, B{};
    
    try {
        auto& a = props.parameters.front();
        auto& b = props.parameters.back();
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<float>(a));
            B = T(Meta::fromStr<float>(b));
            
        } else {
            if (utils::beginsWith(a, '[') && utils::endsWith(a, ']'))
                A = Meta::fromStr<T>(a);
            else
                A = T(Meta::fromStr<float>(a));
            
            if (utils::beginsWith(b, '[') && utils::endsWith(b, ']'))
                B = Meta::fromStr<T>(b);
            else
                B = T(Meta::fromStr<float>(b));
        }

    } catch(const std::exception& ex) {
        throw InvalidSyntaxException("Cannot parse ", props,": ", ex.what());
    }
    
    return apply(A, B);
}

template<typename T>
T map_vector(const VarProps& props, auto&& apply) {
    if(props.parameters.size() != 1) {
        throw InvalidArgumentException("Invalid number of variables for vectorAdd: ", props);
    }
    
    T A{};
    
    try {
        auto& a = props.parameters.front();
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<float>(a));
            
        } else {
            if (utils::beginsWith(a, '[') && utils::endsWith(a, ']'))
                A = Meta::fromStr<T>(a);
            else
                A = T(Meta::fromStr<float>(a));
        }

    } catch(const std::exception& ex) {
        throw InvalidSyntaxException("Cannot parse ", props,": ", ex.what());
    }
    
    return apply(A);
}

void Context::init() const {
    if(system_variables.empty())
        system_variables = {
            VarFunc("min", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return min(A, B);});
            }),
            VarFunc("max", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return max(A, B);});
            }),
            VarFunc("not", [](VarProps props) -> bool {
                if(props.parameters.size() != 1) {
                    throw InvalidArgumentException("Invalid number of variables for not: ", props);
                }
                
                auto& p = props.parameters.front();
                try {
                    return not Meta::fromStr<bool>(p);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p, ": ", ex.what());
                }
            }),
            VarFunc("+", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A+B;});
            }),
            VarFunc("-", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A-B;});
            }),
            VarFunc("*", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A * B;});
            }),
            VarFunc("/", [](VarProps props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A / B;});
            }),
            VarFunc("addVector", [](VarProps props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A + B; });
            }),
            VarFunc("mulVector", [](VarProps props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divVector", [](VarProps props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("mulSize", [](VarProps props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divSize", [](VarProps props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("minVector", [](VarProps props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return cmn::min(A, B); });
            }),
            VarFunc("addSize", [](VarProps props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A + B; });
            }),
            VarFunc("round", [](VarProps props) -> float {
                return map_vector<float>(props, [](auto& A){ return std::roundf(A); });
            }),
            VarFunc("global", [](VarProps) -> sprite::Map& { return GlobalSettings::map(); }),
            VarFunc("clrAlpha", [](VarProps props) -> Color {
                if(props.parameters.size() != 2) {
                    throw InvalidArgumentException("Invalid number of variables for vectorAdd: ", props);
                }
                
                auto& _color = props.parameters.front();
                auto& _alpha = props.parameters.back();
                
                try {
                    auto color = Meta::fromStr<Color>(_color);
                    auto alpha = Meta::fromStr<float>(_alpha);
                    
                    return color.alpha(alpha);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse color ", _color, " and alpha ", _alpha, ": ", ex.what());
                }
            })
        };
    
}

const std::shared_ptr<VarBase_t>& Context::variable(const std::string_view & name) const {
    auto it = variables.find(name);
    if(it != variables.end())
        return it->second;
    
    init();
    
    auto sit = system_variables.find(name);
    if(sit != system_variables.end())
        return sit->second;
    
    throw InvalidArgumentException("Cannot find key ", name, " in variables.");
}
namespace Modules {
std::unordered_map<std::string, Module> mods;

void add(Module&& m) {
    auto name = m._name;
    mods.emplace(name, std::move(m));
}

void remove(const std::string& name) {
    if(mods.contains(name))
        mods.erase(name);
}

Module* exists(const std::string& name) {
    auto it = mods.find(name);
    if(it != mods.end()) {
        return &it->second;
    }
    return nullptr;
}

}

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state,
                         const DefaultSettings& defaults,
                         uint64_t hash)
{
    LayoutContext layout(obj, state, defaults, hash);
    try {
        Layout::Ptr ptr;

        switch (layout.type) {
            case LayoutType::each:
                ptr = layout.create_object<LayoutType::each>(context);
                break;
            case LayoutType::list:
                ptr = layout.create_object<LayoutType::list>(context);
                break;
            case LayoutType::condition:
                ptr = layout.create_object<LayoutType::condition>(context);
                break;
            case LayoutType::vlayout:
                ptr = layout.create_object<LayoutType::vlayout>(context);
                break;
            case LayoutType::hlayout:
                ptr = layout.create_object<LayoutType::hlayout>(context);
                break;
            case LayoutType::gridlayout:
                ptr = layout.create_object<LayoutType::gridlayout>(context);
                break;
            case LayoutType::collection:
                ptr = layout.create_object<LayoutType::collection>(context);
                break;
            case LayoutType::button:
                ptr = layout.create_object<LayoutType::button>(context);
                break;
            case LayoutType::circle:
                ptr = layout.create_object<LayoutType::circle>(context);
                break;
            case LayoutType::text:
                ptr = layout.create_object<LayoutType::text>(context);
                break;
            case LayoutType::stext:
                ptr = layout.create_object<LayoutType::stext>(context);
                break;
            case LayoutType::rect:
                ptr = layout.create_object<LayoutType::rect>(context);
                break;
            case LayoutType::textfield:
                ptr = layout.create_object<LayoutType::textfield>(context);
                break;
            case LayoutType::checkbox:
                ptr = layout.create_object<LayoutType::checkbox>(context);
                break;
            case LayoutType::settings:
                ptr = layout.create_object<LayoutType::settings>(context);
                break;
            case LayoutType::combobox:
                ptr = layout.create_object<LayoutType::combobox>(context);
                break;
            case LayoutType::image:
                ptr = layout.create_object<LayoutType::image>(context);
                break;
            default:
                FormatExcept("Unknown layout type: ", layout.type);
                break;
        }
        
        if(not ptr) {
            FormatExcept("Cannot create object ",obj["type"].get<std::string>());
            return nullptr;
        }
        
        layout.finalize(ptr);
        return ptr;
    } catch(const std::exception& e) {
        std::string text = "Failed to make object with '"+std::string(e.what())+"' for "+ obj.dump();
        FormatExcept("Failed to make object here ",e.what(), " for ",obj.dump());
        return Layout::Make<ErrorElement>(attr::Str{text}, Loc{layout.pos}, Size{layout.size});
    }
}

std::string parse_text(const std::string_view& pattern, const Context& context) {
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
                        throw InvalidSyntaxException("Invalid escape sequence at position ", i, ": ", ch, ". Only braces need to be escaped.");
                    }
                    output << ch;
            }
        } else {
            if(ch == '}') {
                if(nesting_start_positions.empty()) {
                    throw InvalidSyntaxException("Mismatched closing brace at position ", i);
                }
                
                std::size_t start_pos = nesting_start_positions.top();
                nesting_start_positions.pop();

                std::string_view current_word = pattern.substr(start_pos, i - start_pos);
                if(current_word.empty()) {
                    throw InvalidSyntaxException("Empty braces at position ", i);
                }

                std::string resolved_word = resolve_variable(current_word, context, [](const VarBase_t& variable, const VarProps& modifiers) -> std::string {
                        try {
                            std::string ret;
                            if(variable.is<Size2>()) {
                                if(modifiers.subs.empty())
                                    ret = variable.value_string(modifiers);
                                else if(modifiers.subs.front() == "w")
                                    ret = Meta::toStr(variable.value<Size2>(modifiers).width);
                                else if(modifiers.subs.front() == "h")
                                    ret = Meta::toStr(variable.value<Size2>(modifiers).height);
                                else
                                    throw InvalidArgumentException("Sub ",modifiers," of Size2 is not valid.");
                                
                            } else if(variable.is<Vec2>()) {
                                if(modifiers.subs.empty())
                                    ret = variable.value_string(modifiers);
                                else if(modifiers.subs.front() == "x")
                                    ret = Meta::toStr(variable.value<Vec2>(modifiers).x);
                                else if(modifiers.subs.front() == "y")
                                    ret = Meta::toStr(variable.value<Vec2>(modifiers).y);
                                else
                                    throw InvalidArgumentException("Sub ",modifiers," of Vec2 is not valid.");
                                
                            } else
                                ret = variable.value_string(modifiers);
                            //auto str = modifiers.toStr();
                            //print(str.c_str(), " resolves to ", ret);
                            if(modifiers.html)
                                return settings::htmlify(ret);
                            return ret;
                        } catch(const std::exception& ex) {
                            FormatExcept("Exception: ", ex.what());
                            return modifiers.optional ? "" : "null";
                        }
                    }, [](bool optional) -> std::string {
                        return optional ? "" : "null";
                    });
                if(nesting_start_positions.empty()) {
                    output << resolved_word;
                } else {
                    //nesting.top() += resolved_word;
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

tl::expected<std::tuple<DefaultSettings, nlohmann::json>, const char*> load(const file::Path& path){
    static Timer timer;
    if(timer.elapsed() < 0.15) {
        return tl::unexpected("Have to wait longer to reload.");
    }
    timer.reset();
    
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        DefaultSettings defaults;
        try {
            auto obj = nlohmann::json::parse(text);
            try {
                if(obj.contains("defaults") && obj["defaults"].is_object()) {
                    auto d = obj["defaults"];
                    defaults.font = parse_font(d, defaults.font);
                    if(d.contains("color")) defaults.textClr = parse_color(d["color"]);
                    if(d.contains("fill")) defaults.fill = parse_color(d["fill"]);
                    if(d.contains("line")) defaults.line = parse_color(d["line"]);
                    if(d.contains("highlight_clr")) defaults.highlightClr = parse_color(d["highlight_clr"]);
                    if(d.contains("window_color")) defaults.window_color = parse_color(d["window_color"]);
                    if(d.contains("pad"))
                        defaults.pad = Meta::fromStr<Bounds>(d["pad"].dump());
                }
            } catch(const std::exception& ex) {
                FormatExcept("Cannot parse layout due to: ", ex.what());
                return tl::unexpected(ex.what());
            }
            return std::make_tuple(defaults, obj["objects"]);
            
        } catch(const nlohmann::json::exception& error) {
            return tl::unexpected(error.what());
        }
        
    } else
        return tl::unexpected("Nothing changed.");
}

bool DynamicGUI::update_objects(DrawStructure& g, Layout::Ptr& o, const Context& context, State& state) {
    auto hash = (std::size_t)o->custom_data("object_index");
    if(not hash) {
        //! if this is a Layout type, need to iterate all children as well:
        if(o.is<Layout>()) {
            auto layout = o.to<Layout>();
            auto& objects = layout->objects();
            for(size_t i=0, N = objects.size(); i<N; ++i) {
                auto& child = objects[i];
                auto r = update_objects(g, child, context, state);
                if(r) {
                    // objects changed
                    layout->replace_child(i, child);
                }
            }
        } else {
            //print("Object ", *o, " has no hash and is thus not updated.");
        }
        return false;
    }
    //print("updating ", o->type()," with index ", hash, " for ", (uint64_t)o.get(), " ", o->name());
    
    //! something that needs to be executed before everything runs
    if(state.display_fns.contains(hash)) {
        state.display_fns.at(hash)(g);
    }
    
    //! if statements below
    if(auto it = state.ifs.find(hash); it != state.ifs.end()) {
        auto &obj = it->second;
        try {
            auto res = resolve_variable_type<bool>(obj.variable, context);
            if(not res) {
                if(obj._else) {
                    auto last_condition = (uint64_t)o->custom_data("last_condition");
                    if(last_condition != 1) {
                        o->add_custom_data("last_condition", (void*)1);
                        o.to<Layout>()->set_children({obj._else});
                    }
                    update_objects(g, obj._else, context, state);
                    
                } else {
                    if(o->is_displayed()) {
                        o->set_is_displayed(false);
                        o.to<Layout>()->clear_children();
                    }
                }
                
                return false;
            }
            
            auto last_condition = (uint64_t)o->custom_data("last_condition");
            if(last_condition != 2) {
                o->set_is_displayed(true);
                o->add_custom_data("last_condition", (void*)2);
                o.to<Layout>()->set_children({obj._if});
            }
            update_objects(g, obj._if, context, state);
            
        } catch(const std::exception& ex) {
            FormatError(ex.what());
        }
        
        return false;
    }
    
    //! for-each loops below
    if(update_loops(hash, g, o, context, state))
        return false;
    
    //! did the current object change?
    bool changed{false};
    
    //! fill default fields like fill, line, pos, etc.
    if(update_patterns(hash, o, context, state)) {
        changed = true;
    }
    
    //! if this is a Layout type, need to iterate all children as well:
    if(o.is<Layout>()) {
        auto layout = o.to<Layout>();
        auto& objects = layout->objects();
        for(size_t i=0, N = objects.size(); i<N; ++i) {
            auto& child = objects[i];
            auto r = update_objects(g, child, context, state);
            if(r) {
                // objects changed
                layout->replace_child(i, child);
            }
        }
    }
    
    //! list contents loops below
    (void)update_lists(hash, g, o, context, state);
    return changed;
}

bool DynamicGUI::update_patterns(uint64_t hash, Layout::Ptr &o, const Context &context, State &state) {
    bool changed{false};
    
    auto it = state.patterns.find(hash);
    if(it != state.patterns.end()) {
        auto pattern = it->second;
        LabeledField *field{nullptr};
        if(state._text_fields.contains(hash))
            field = state._text_fields.at(hash).get();
        
        //print("Found pattern ", pattern, " at index ", hash);
        
        if(it->second.contains("var")) {
            try {
                auto var = parse_text(pattern.at("var"), context);
                if(state._text_fields.contains(hash)) {
                    auto &f = state._text_fields.at(hash);
                    auto str = f->_ref.get().valueString();
                    VarCache &cache = state._var_cache[hash];
                    if(cache._var != var || str != cache._value)
                    {
                        print("Need to change ", cache._value," => ", str, " and ", cache._var, " => ", var);
                        
                        //! replace old object (also updates the cache)
                        o = parse_object(cache._obj, context, state, context.defaults, hash);
                        
                        field = f.get();
                        changed = true;
                    }
                }
            } catch(std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        auto ptr = o;
        if(field) {
            ptr = field->representative();
        }
        
        if(it->second.contains("fill")) {
            try {
                auto fill = resolve_variable_type<Color>(pattern.at("fill"), context);
                LabeledField::delegate_to_proper_type(FillClr{fill}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("color")) {
            try {
                auto clr = resolve_variable_type<Color>(pattern.at("color"), context);
                LabeledField::delegate_to_proper_type(TextClr{clr}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("line")) {
            try {
                auto line = resolve_variable_type<Color>(pattern.at("line"), context);
                LabeledField::delegate_to_proper_type(LineClr{line}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        
        if(pattern.contains("pos")) {
            try {
                auto pos = pattern.at("pos");
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
                auto str = parse_text(pos, context);
                ptr->set_pos(Meta::fromStr<Vec2>(str));
                //o->set_pos(resolve_variable_type<Vec2>(pattern.at("pos"), context));
                //
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("scale")) {
            try {
                ptr->set_scale(Meta::fromStr<Vec2>(parse_text(pattern.at("scale"), context)));
                //o->set_scale(resolve_variable_type<Vec2>(pattern.at("scale"), context));
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("size")) {
            try {
                auto size = pattern.at("size");
                if(size == "auto")
                {
                    if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                    else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
                } else {
                    ptr->set_size(Meta::fromStr<Size2>(parse_text(size, context)));
                    //o->set_size(resolve_variable_type<Size2>(size, context));
                }
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("text")) {
            try {
                auto text = Str{parse_text(pattern.at("text"), context)};
                LabeledField::delegate_to_proper_type(text, ptr);
            } catch(const std::exception& e) {
                FormatError("Error parsing context ", pattern.at("text"),": ", e.what());
            }
        }
        
        if(ptr.is<ExternalImage>()) {
            if(pattern.contains("path")) {
                auto img = ptr.to<ExternalImage>();
                auto output = file::Path(parse_text(pattern.at("path"), context));
                if(output.exists() && not output.is_folder()) {
                    auto modified = output.last_modified();
                    auto &entry = state._image_cache[output.str()];
                    Image::Ptr ptr;
                    if(std::get<0>(entry) != modified) {
                        ptr = load_image(output);
                        entry = { modified, Image::Make(*ptr) };
                    }
                    
                    if(ptr) {
                        img->set_source(std::move(ptr));
                    }
                }
            }
        }
    }
    return changed;
}

bool DynamicGUI::update_lists(uint64_t hash, DrawStructure &, const Layout::Ptr &o, const Context &context, State &state)
{
    if(auto it = state.lists.find(hash); it != state.lists.end()) {
        ListContents &obj = it->second;
        if(context.has(obj.variable)) {
            if(context.variable(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                
                auto& vector = context.variable(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>({});
                
                IndexScopeHandler handler{state._current_index};
                //if(vector != obj.cache) {
                std::vector<DetailItem> ptrs;
                //obj.cache = vector;
                obj.state = std::make_unique<State>();
                Context tmp = context;
                
                size_t index=0;
                auto convert_to_item = [&gc = context, &index, &obj, &o](sprite::Map&, const nlohmann::json& item_template, Context& context) -> DetailItem
                {
                    DetailItem item;
                    if(item_template.contains("text") && item_template["text"].is_string()) {
                        item.set_name(parse_text(item_template["text"].get<std::string>(), context));
                    }
                    if(item_template.contains("detail") && item_template["detail"].is_string()) {
                        item.set_detail(parse_text(item_template["detail"].get<std::string>(), context));
                    }
                    if(item_template.contains("action") && item_template["action"].is_string()) {
                        auto action = Action::fromStr(parse_text(item_template["action"].get<std::string>(), context));
                        
                        if(not obj.on_select_actions.contains(index)
                           || std::get<0>(obj.on_select_actions.at(index)) != action.name)
                        {
                            obj.on_select_actions[index] = std::make_tuple(
                                index, [&gc = gc, ptr = o.get(), index = index, action = action](){
                                    print("Clicked item at ", index, " with action ", action);
                                    Action _action = action;
                                    _action.parameters = { Meta::toStr(index) };
                                    if(gc.actions.contains(action.name)) {
                                        gc.actions.at(action.name)(_action);
                                    }
                                }
                            );
                        }
                    }
                    return item;
                };
                
                for(auto &v : vector) {
                    tmp.variables["i"] = v;
                    try {
                        auto &ref = v->value<sprite::Map&>({});
                        auto item = convert_to_item(ref, obj.item, tmp);
                        ptrs.emplace_back(std::move(item));
                        ++index;
                    } catch(const std::exception& ex) {
                        FormatExcept("Cannot create list items for template: ", obj.item.dump(), " and type ", v->class_name());
                    }
                }
                
                o.to<ScrollableList<DetailItem>>()->set_items(ptrs);
            }
        }
        return true;
    }
    return false;
}

bool DynamicGUI::update_loops(uint64_t hash, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state)
{
    if(auto it = state.loops.find(hash); it != state.loops.end()) {
        auto &obj = it->second;
        if(context.has(obj.variable)) {
            if(context.variable(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                
                auto& vector = context.variable(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>({});
                
                IndexScopeHandler handler{state._current_index};
                if(vector != obj.cache) {
                    std::vector<Layout::Ptr> ptrs;
                    obj.cache = vector;
                    obj.state = std::make_unique<State>();
                    Context tmp = context;
                    for(auto &v : vector) {
                        tmp.variables["i"] = v;
                        auto ptr = parse_object(obj.child, tmp, *obj.state, context.defaults);
                        update_objects(g, ptr, tmp, *obj.state);
                        ptrs.push_back(ptr);
                    }
                    
                    o.to<Layout>()->set_children(ptrs);
                    
                } else {
                    Context tmp = context;
                    for(size_t i=0; i<obj.cache.size(); ++i) {
                        tmp.variables["i"] = obj.cache[i];
                        auto& p = o.to<Layout>()->objects().at(i);
                        if(p)
                            update_objects(g, p, tmp, *obj.state);
                        //p->parent()->stage()->print(nullptr);
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void DynamicGUI::reload() {
    if(not first_load
       && last_update.elapsed() < 0.25)
    {
        return;
    }
    
    if(first_load)
        first_load = false;
    
    try {
        auto cwd = file::cwd().absolute();
        auto app = file::DataLocation::parse("app").absolute();
        if(cwd != app) {
            print("check_module:CWD: ", cwd);
            file::cd(app);
        }
        
        auto result = load(path);
        if(result) {
            state._text_fields.clear();
            
            auto [defaults, layout] = result.value();
            context.defaults = defaults;
            
            std::vector<Layout::Ptr> objs;
            State tmp;
            for(auto &obj : layout) {
                auto ptr = parse_object(obj, context, tmp, context.defaults);
                if(ptr) {
                    objs.push_back(ptr);
                }
            }
            
            state = std::move(tmp);
            objects = std::move(objs);
        }
        
    } catch(const std::invalid_argument&) {
    } catch(const std::exception& e) {
        FormatExcept("Error loading gui layout from file: ", e.what());
    } catch(...) {
        FormatExcept("Error loading gui layout from file");
    }
    
    last_update.reset();
}

void DynamicGUI::update(Layout* parent, const std::function<void(std::vector<Layout::Ptr>&)>& before_add) 
{
    reload();
    
    if(context.defaults.window_color != Transparent) {
        if(base)
            base->set_background_color(context.defaults.window_color);
        else {
            static std::once_flag flag;
            std::call_once(flag, [&](){
                FormatWarning("Cannot set background color to ", context.defaults.window_color);
            });
        }
    }
    
    if(parent) {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            parent->set_children(copy);
        } else {
            parent->set_children(objects);
        }
        
        for(auto &obj : objects) {
            update_objects(*graph, obj, context, state);
        }
        
    } else {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            for(auto &obj : copy) {
                update_objects(*graph, obj, context, state);
                graph->wrap_object(*obj);
            }
            
        } else {
            for(auto &obj : objects) {
                update_objects(*graph, obj, context, state);
                graph->wrap_object(*obj);
            }
        }
    }
    
    dyn::update_tooltips(*graph, state);
}

DynamicGUI::operator bool() const {
    return graph != nullptr;
}

void DynamicGUI::clear() {
    graph = nullptr;
    objects.clear();
    state = {};
}

void update_tooltips(DrawStructure& graph, State& state) {
    if(not state._settings_tooltip) {
        state._settings_tooltip = Layout::Make<SettingsTooltip>();
    }
    
    Layout::Ptr found = nullptr;
    std::string name;
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto & [key, ptr] : state._text_fields) {
        if(not ptr)
            continue;
        
        ptr->_text->set_clickable(true);
        
        if(ptr->representative()->hovered() && (ptr->representative().ptr.get() == graph.hovered_object() || dynamic_cast<Textfield*>(graph.hovered_object()))) {
            found = ptr->representative();
            name = ptr->_ref.get().name();
            break;
        }
    }
    
    if(found) {
        ref = std::make_unique<sprite::Reference>(dyn::settings_map()[name]);
    }

    if(found && ref) {
        state._settings_tooltip.to<SettingsTooltip>()->set_parameter(name);
        state._settings_tooltip.to<SettingsTooltip>()->set_other(found.get());
        graph.wrap_object(*state._settings_tooltip);
    } else {
        state._settings_tooltip.to<SettingsTooltip>()->set_other(nullptr);
    }
}

}
}
