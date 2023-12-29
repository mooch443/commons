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
#include <gui/LabeledField.h>
#include <gui/dyn/ParseText.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>
#include <regex>

namespace gui {
namespace dyn {

template<typename T, typename Str>
T resolve_variable_type(Str _word, const Context& context, State& state) {
    std::string_view word;
    if constexpr(std::same_as<Str, Pattern>) {
        word = std::string_view(_word.original);
    } else
        word = std::string_view(_word);
    
    if(word.empty())
        throw U_EXCEPTION("Invalid variable name (is empty).");
    
    if(word.length() > 2
       && ((word.front() == '{'
            && word.back() == '}')
       || (word.front() == '['
            && word.back() == ']'))
       )
    {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, state, [&](const VarBase_t& variable, const VarProps& modifiers) -> T
    {
        if constexpr(std::is_same_v<T, bool>) {
            if(variable.is<bool>()) {
                return variable.value<T>(modifiers);
            } else if(variable.is<file::Path>()) {
                auto tmp = variable.value<file::Path>(modifiers);
                return not tmp.empty();
            } else if(variable.is<std::string>()) {
                auto tmp = variable.value<std::string>(modifiers);
                return not tmp.empty();
            } else if(variable.is<sprite::Map&>()) {
                auto& tmp = variable.value<sprite::Map&>({});
                if(modifiers.subs.empty())
                    throw U_EXCEPTION("sprite::Map selector is empty, need subvariable doing ", word,".sub");
                
                auto ref = tmp[modifiers.subs.front()];
                if(not ref.get().valid()) {
                    FormatWarning("Retrieving invalid property (", modifiers, ") from map with keys ", tmp.keys(), ".");
                    return false;
                }
                //print(ref.get().name()," = ", ref.get().valueString(), " with ", ref.get().type_name());
                if(ref.template is_type<T>()) {
                    return ref.get().template value<T>();
                } else if(ref.is_type<std::string>()) {
                    return not ref.get().value<std::string>().empty();
                } else if(ref.is_type<file::Path>()) {
                    return not ref.get().value<file::Path>().empty();
                }
            }
        }
        
        if(variable.is<T>()) {
            return variable.value<T>(modifiers);
            
        } else if(variable.is<sprite::Map&>()) {
            auto& tmp = variable.value<sprite::Map&>({});
            if(not modifiers.subs.empty()) {
                auto ref = tmp[modifiers.subs.front()];
                if(ref.template is_type<T>()) {
                    return ref.get().template value<T>();
                }
                
            } else
                throw U_EXCEPTION("sprite::Map selector is empty, need subvariable doing ", word,".sub");
            
        } else if(variable.is<VarReturn_t>()) {
            auto tmp = variable.value<VarReturn_t>(modifiers);
            if(tmp.template is_type<T>()) {
                return tmp.get().template value<T>();
            }
        }
        
        throw U_EXCEPTION("Invalid variable type for ",word," or variable not found.");
        
    }, [&]() -> T {
        throw U_EXCEPTION("Invalid variable type for ",word," or variable not found.");
    });
}

std::string Pattern::toStr() const {
    return "Pattern<" + std::string(original) + ">";
}

bool Context::has(const std::string_view & name) const noexcept {
    if(variables.contains(name))
        return true;
    return system_variables().contains(name);
}

template<typename T>
T map_vectors(const VarProps& props, auto&& apply) {
    if(props.parameters.size() != 2) {
        throw InvalidArgumentException("Invalid number of variables for vectorAdd: ", props);
    }
    
    T A{}, B{};
    
    try {
        std::string a(props.parameters.front());
        std::string b(props.parameters.back());
        
        if constexpr(std::is_floating_point_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<float>(a));
            B = T(Meta::fromStr<float>(b));
        } else if constexpr(are_the_same<bool, T>) {
            A = convert_to_bool(a);
            B = convert_to_bool(b);
            
        } else if constexpr(std::is_integral_v<std::remove_cvref_t<T>>) {
            A = T(Meta::fromStr<int64_t>(a));
            B = T(Meta::fromStr<int64_t>(b));
            
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
        return T{};
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
        std::string a(props.parameters.front());
        
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
    if(not _system_variables.has_value())
        _system_variables = {
            VarFunc("min", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return min(A, B);});
            }),
            VarFunc("max", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return max(A, B);});
            }),
            VarFunc("bool", [](const VarProps& props) -> bool {
                if (props.parameters.size() != 1) {
                    throw InvalidArgumentException("Invalid number of variables for not: ", props);
                }

                return convert_to_bool(props.parameters.front());
            }),
            VarFunc("abs", [](const VarProps& props) -> float {
                if (props.parameters.size() != 1) {
                    throw InvalidArgumentException("Invalid number of variables for abs: ", props);
                }
                auto v = Meta::fromStr<float>(props.parameters.front());
                if(std::isnan(v))
                    return v;
                return fabsf(v);
            }),
            VarFunc("int", [](const VarProps& props) -> int64_t {
                if (props.parameters.size() != 1) {
                    throw InvalidArgumentException("Invalid number of variables for int: ", props);
                }
                auto v = Meta::fromStr<float>(props.parameters.front());
                if(std::isnan(v))
                    return static_cast<int64_t>(std::numeric_limits<int64_t>::quiet_NaN());
                else
                    return static_cast<int64_t>(v);
            }),
            VarFunc("dec", [](const VarProps& props) -> std::string {
                if (props.parameters.size() != 2) {
                    throw InvalidArgumentException("Invalid number of variables for dec: ", props);
                }

                float decimals = Meta::fromStr<uint8_t>(props.parameters.front());
                
                auto str = props.parameters.back();
                auto it = std::find(str.begin(), str.end(), '.');
                if(it != str.end()) {
                    size_t offset = 0;
                    while(++it != str.end() && offset++ < decimals) {}
                    str.erase(it, str.end());
                }
                
                return str;
            }),
            VarFunc("not", [](const VarProps& props) -> bool {
                if(props.parameters.size() != 1) {
                    throw InvalidArgumentException("Invalid number of variables for not: ", props);
                }
                
                std::string p(props.parameters.front());
                try {
                    return not Meta::fromStr<bool>(p);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p, ": ", ex.what());
                }
            }),
            VarFunc("equal", [](const VarProps& props) -> bool {
                if(props.parameters.size() != 2) {
                    throw InvalidArgumentException("Invalid number of variables for ",props.name,": ", props);
                }
                
                std::string p0(props.parameters.front());
                std::string p1(props.parameters.back());
                try {
                    return p0 == p1;
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse boolean ", p0, " == ",p1,": ", ex.what());
                }
            }),
            VarFunc("&&", [](const VarProps& props) -> bool {
                for (auto &c : props.parameters) {
                    if(not convert_to_bool(c))
                        return false;
                }
                return true;
            }),
            VarFunc("||", [](const VarProps& props) -> bool {
                for (auto &c : props.parameters) {
                    if(convert_to_bool(c))
                        return true;
                }
                return false;
            }),
            VarFunc(">", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A > B;});
            }),
            VarFunc(">=", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A >= B;});
            }),
            VarFunc("<", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A < B;});
            }),
            VarFunc("<=", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A <= B;});
            }),
            VarFunc("+", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A+B;});
            }),
            VarFunc("-", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A-B;});
            }),
            VarFunc("*", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A * B;});
            }),
            VarFunc("/", [](const VarProps& props) -> float {
                return map_vectors<float>(props, [](auto&A, auto&B){return A / B;});
            }),
            VarFunc("addVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A + B; });
            }),
            VarFunc("subVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A - B; });
            }),
            VarFunc("mulVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("mulSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.mul(B); });
            }),
            VarFunc("divSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A.div(B); });
            }),
            VarFunc("minVector", [](const VarProps& props) -> Vec2 {
                return map_vectors<Vec2>(props, [](auto& A, auto& B){ return cmn::min(A, B); });
            }),
            VarFunc("addSize", [](const VarProps& props) -> Size2 {
                return map_vectors<Size2>(props, [](auto& A, auto& B){ return A + B; });
            }),
            VarFunc("round", [](const VarProps& props) -> float {
                return map_vector<float>(props, [](auto& A){ return std::roundf(A); });
            }),
            VarFunc("at", [](const VarProps& props) -> std::string {
                if(props.parameters.size() != 2) {
                    throw InvalidArgumentException("Invalid number of variables for at: ", props);
                }
                
                auto index = Meta::fromStr<size_t>(props.parameters.front());
                auto parts = util::parse_array_parts(util::truncate(props.parameters.at(1)));
                return parts.at(index);
            }),
            VarFunc("global", [](const VarProps&) -> sprite::Map& { return GlobalSettings::map(); }),
            VarFunc("clrAlpha", [](const VarProps& props) -> Color {
                if(props.parameters.size() != 2) {
                    throw InvalidArgumentException("Invalid number of variables for vectorAdd: ", props);
                }
                
                std::string _color(props.parameters.front());
                std::string _alpha(props.parameters.back());
                
                try {
                    auto color = Meta::fromStr<Color>(_color);
                    auto alpha = Meta::fromStr<float>(_alpha);
                    
                    return color.alpha(alpha);
                    
                } catch(const std::exception& ex) {
                    throw InvalidArgumentException("Cannot parse color ", _color, " and alpha ", _alpha, ": ", ex.what());
                }
            }),
            
            VarFunc("shorten", [](const VarProps& props) -> std::string {
                if(props.parameters.size() < 1 || props.parameters.size() > 3) {
                    throw InvalidArgumentException("Need one to three arguments for ", props,".");
                }
                
                std::string str = props.first();
                size_t N = 50;
                std::string placeholder = "…";
                if(props.parameters.size() > 1)
                    N = Meta::fromStr<size_t>(props.parameters.at(1));
                if(props.parameters.size() > 2)
                    placeholder = props.last();
                
                return utils::ShortenText(str, N, 0.5, placeholder);
            }),
            VarFunc("filename", [](const VarProps& props) -> file::Path {
                if(props.parameters.size() != 1) {
                    throw InvalidArgumentException("Need one argument for ", props,".");
                }
                return file::Path(Meta::fromStr<file::Path>(props.first()).filename());
            }),
            VarFunc("folder", [](const VarProps& props) -> file::Path {
                if(props.parameters.size() != 1) {
                    throw InvalidArgumentException("Need one argument for ", props,".");
                }
                return file::Path(Meta::fromStr<file::Path>(props.first()).remove_filename());
            }),
            VarFunc("basename", [](const VarProps& props) -> file::Path {
                if(props.parameters.size() != 1) {
                    throw InvalidArgumentException("Need one argument for ", props,".");
                }
                return file::Path(Meta::fromStr<file::Path>(props.first()).filename()).remove_extension();
            })
        };
    
}

const std::shared_ptr<VarBase_t>& Context::variable(const std::string_view & name) const {
    auto it = variables.find(name);
    if(it != variables.end())
        return it->second;
    
    init();
    
    auto sit = system_variables().find(name);
    if(sit != system_variables().end())
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
    LayoutContext layout(obj, state, context, defaults, hash);
    hash = layout.hash;

    try {
        Layout::Ptr ptr;

        switch (layout.type) {
            case LayoutType::each:
                ptr = layout.create_object<LayoutType::each>();
                break;
            case LayoutType::list:
                ptr = layout.create_object<LayoutType::list>();
                break;
            case LayoutType::condition:
                ptr = layout.create_object<LayoutType::condition>();
                break;
            case LayoutType::vlayout:
                ptr = layout.create_object<LayoutType::vlayout>();
                break;
            case LayoutType::hlayout:
                ptr = layout.create_object<LayoutType::hlayout>();
                break;
            case LayoutType::gridlayout:
                ptr = layout.create_object<LayoutType::gridlayout>();
                break;
            case LayoutType::collection:
                ptr = layout.create_object<LayoutType::collection>();
                break;
            case LayoutType::button:
                ptr = layout.create_object<LayoutType::button>();
                break;
            case LayoutType::circle:
                ptr = layout.create_object<LayoutType::circle>();
                break;
            case LayoutType::text:
                ptr = layout.create_object<LayoutType::text>();
                break;
            case LayoutType::stext:
                ptr = layout.create_object<LayoutType::stext>();
                break;
            case LayoutType::rect:
                ptr = layout.create_object<LayoutType::rect>();
                break;
            case LayoutType::textfield:
                ptr = layout.create_object<LayoutType::textfield>();
                break;
            case LayoutType::checkbox:
                ptr = layout.create_object<LayoutType::checkbox>();
                break;
            case LayoutType::settings:
                ptr = layout.create_object<LayoutType::settings>();
                break;
            case LayoutType::combobox:
                ptr = layout.create_object<LayoutType::combobox>();
                break;
            case LayoutType::image:
                ptr = layout.create_object<LayoutType::image>();
                break;
            default:
                if (auto it = context.custom_elements.find(obj["type"].get<std::string>());
                    it != context.custom_elements.end()) 
                {
                    if (state._customs_cache.contains(hash)) {
                        ptr = state._customs_cache.at(hash);
                        context.custom_elements.at(state._customs.at(hash))
                            .update(ptr, context, state, state.patterns.contains(hash) ? state.patterns.at(hash) : decltype(state.patterns)::mapped_type{});
                        
                    } else {
                        ptr = it->second.create(layout);
                        it->second
                            .update(ptr, context, state, state.patterns.contains(hash) ? state.patterns.at(hash) : decltype(state.patterns)::mapped_type{});
                        state._customs[hash] = it->first;
                        state._customs_cache[hash] = ptr;
                    }
                } else
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

tl::expected<std::tuple<DefaultSettings, nlohmann::json>, const char*> load(const std::string& text){
    DefaultSettings defaults;
    try {
        auto obj = nlohmann::json::parse(text);
        State state;
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
                
                if(d.contains("vars") && d["vars"].is_object()) {
                    for(auto &[name, value] : d["vars"].items()) {
                        defaults.variables[name] = std::unique_ptr<VarBase<const Context&, State&>>(new Variable([value = Meta::fromStr<std::string>(value.dump())](const Context& context, State& state) -> std::string {
                            return parse_text(value, context, state);
                        }));
                    }
                }
            }
        } catch(const std::exception& ex) {
            //FormatExcept("Cannot parse layout due to: ", ex.what());
            //return tl::unexpected(ex.what());
            throw InvalidSyntaxException(ex.what());
        }
        return std::make_tuple(defaults, obj["objects"]);
        
    } catch(const nlohmann::json::exception& error) {
        throw InvalidSyntaxException(error.what());
        //return tl::unexpected(error.what());
    }
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
    
    /*if(auto it = state._timers.find(hash); it != state._timers.end()) {
        if(it->second.elapsed() > 0.01) {
            it->second.reset();
        } else
            return false;
    }*/
    
    if(o) state._current_object = o.get();
    else state._current_object = nullptr;
    
    //! something that needs to be executed before everything runs
    if(state.display_fns.contains(hash)) {
        state.display_fns.at(hash)(g);
    }

    if (state._customs.contains(hash)) {
        context.custom_elements.at(state._customs.at(hash)).update(o, context, state, state.patterns.contains(hash) ? state.patterns.at(hash) : decltype(state.patterns)::mapped_type{});
        return false;
    }
    
    //! if statements below
    if(auto it = state.ifs.find(hash); it != state.ifs.end()) {
        auto &obj = it->second;
        try {
            auto res = resolve_variable_type<bool>(obj.variable, context, state);
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
        auto& pattern = it->second;
        LabeledField *field{nullptr};
        if(state._text_fields.contains(hash))
            field = state._text_fields.at(hash).get();
        
        //print("Found pattern ", pattern, " at index ", hash);
        
        if(it->second.contains("var")) {
            try {
                auto var = parse_text(pattern.at("var"), context, state);
                if(state._text_fields.contains(hash)) {
                    auto &f = state._text_fields.at(hash);
                    auto str = f->_ref.get().valueString();
                    VarCache &cache = state._var_cache[hash];
                    if(cache._var != var /*|| str != cache._value*/)
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
                auto fill = resolve_variable_type<Color>(pattern.at("fill"), context, state);
                LabeledField::delegate_to_proper_type(FillClr{fill}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("color")) {
            try {
                auto clr = resolve_variable_type<Color>(pattern.at("color"), context, state);
                LabeledField::delegate_to_proper_type(TextClr{clr}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("line")) {
            try {
                auto line = resolve_variable_type<Color>(pattern.at("line"), context, state);
                LabeledField::delegate_to_proper_type(LineClr{line}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        
        if(pattern.contains("pos")) {
            try {
                auto str = parse_text(pattern.at("pos"), context, state);
                ptr->set_pos(Meta::fromStr<Vec2>(str));
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("scale")) {
            try {
                ptr->set_scale(Meta::fromStr<Vec2>(parse_text(pattern.at("scale"), context, state)));
                //o->set_scale(resolve_variable_type<Vec2>(pattern.at("scale"), context));
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("size")) {
            try {
                auto& size = pattern.at("size");
                if(size.original == "auto")
                {
                    if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                    else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
                } else {
                    ptr->set_size(Meta::fromStr<Size2>(parse_text(size, context, state)));
                    //o->set_size(resolve_variable_type<Size2>(size, context));
                }
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(it->second.contains("max_size")) {
            try {
                auto line = Meta::fromStr<Size2>(parse_text(pattern.at("max_size"), context, state));
                LabeledField::delegate_to_proper_type(SizeLimit{line}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if (it->second.contains("item_line")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("item_line"), context, state));
                LabeledField::delegate_to_proper_type(ItemBorderColor_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if (it->second.contains("item_color")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("item_color"), context, state));
                LabeledField::delegate_to_proper_type(ItemColor_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if (it->second.contains("label_line")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("label_line"), context, state));
                LabeledField::delegate_to_proper_type(LabelBorderColor_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if (it->second.contains("label_fill")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("label_fill"), context, state));
                LabeledField::delegate_to_proper_type(LabelColor_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("list_size")) {
            try {
                auto line = Meta::fromStr<Size2>(parse_text(pattern.at("list_size"), context, state));
                LabeledField::delegate_to_proper_type(ListDims_t{line}, ptr);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if (it->second.contains("list_line")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("list_line"), context, state));
                LabeledField::delegate_to_proper_type(ListLineClr_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }

        if (it->second.contains("list_fill")) {
            try {
                auto line = Meta::fromStr<Color>(parse_text(pattern.at("list_fill"), context, state));
                LabeledField::delegate_to_proper_type(ListFillClr_t{ line }, ptr);

            }
            catch (const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("text")) {
            try {
                auto text = Str{parse_text(pattern.at("text"), context, state)};
                LabeledField::delegate_to_proper_type(text, ptr);
            } catch(const std::exception& e) {
                FormatError("Error parsing context ", pattern.at("text"),": ", e.what());
            }
        }
        
        if(pattern.contains("radius")) {
            try {
                auto text = Radius{Meta::fromStr<float>(parse_text(pattern.at("radius"), context, state))};
                LabeledField::delegate_to_proper_type(text, ptr);
            } catch(const std::exception& e) {
                FormatError("Error parsing context ", pattern.at("radius"),": ", e.what());
            }
        }

        if(ptr.is<ExternalImage>()) {
            if(pattern.contains("path")) {
                auto img = ptr.to<ExternalImage>();
                auto output = file::Path(parse_text(pattern.at("path"), context, state));
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
                auto convert_to_item = [&gc = context, &index, &obj, &o](sprite::Map&, const nlohmann::json& item_template, Context& context, State& state) -> DetailItem
                {
                    DetailItem item;
                    if(item_template.contains("text") && item_template["text"].is_string()) {
                        item.set_name(parse_text(item_template["text"].get<std::string>(), context, state));
                    }
                    if(item_template.contains("detail") && item_template["detail"].is_string()) {
                        item.set_detail(parse_text(item_template["detail"].get<std::string>(), context, state));
                    }
                    if(item_template.contains("action") && item_template["action"].is_string()) {
                        auto action = PreAction::fromStr(parse_text(item_template["action"].get<std::string>(), context, state));
                        
                        if(not obj.on_select_actions.contains(index)
                           || std::get<0>(obj.on_select_actions.at(index)) != action.name)
                        {
                            obj.on_select_actions[index] = std::make_tuple(
                                index, [&gc = gc, ptr = o.get(), index = index, action = action, context](){
                                    print("Clicked item at ", index, " with action ", action);
                                    State state;
                                    Action _action = action.parse(context, state);
                                    _action.parameters = { Meta::toStr(index) };
                                    if(auto it = gc.actions.find(action.name); it != gc.actions.end()) {
                                        try {
                                            it->second(_action);
                                        } catch(const std::exception& ex) {
                                            // pass
                                        }
                                    }
                                }
                            );
                        }
                    }
                    return item;
                };
                
                for(auto &v : vector) {
                    auto previous = state._variable_values;
                    tmp.variables["i"] = v;
                    try {
                        auto &ref = v->value<sprite::Map&>({});
                        auto item = convert_to_item(ref, obj.item, tmp, state);
                        ptrs.emplace_back(std::move(item));
                        ++index;
                    } catch(const std::exception& ex) {
                        FormatExcept("Cannot create list items for template: ", obj.item.dump(), " and type ", v->class_name());
                    }
                    state._variable_values = std::move(previous);
                }
                
                o.to<ScrollableList<DetailItem>>()->set_items(ptrs);
            }
        }
        return true;
    }
    else if (auto it = state.manual_lists.find(hash); it != state.manual_lists.end()) {
        ManualListContents &body = it->second;
        auto items = body.items;
        for (auto& i : items) {
            i.set_detail(parse_text(i.detail(), context, state));
            i.set_name(parse_text(i.name(), context, state));
        }
        o.to<ScrollableList<DetailItem>>()->set_items(items);
        return true;
    }

    return false;
}

bool DynamicGUI::update_loops(uint64_t hash, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state)
{
    if(auto it = state.loops.find(hash); it != state.loops.end()) {
        auto &obj = it->second;
        PreVarProps ps = extractControls(obj.variable);
        auto props = ps.parse(context, state);
        
        if(context.has(props.name)) {
            if(context.variable(props.name)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                auto& vector = context.variable(props.name)->value<std::vector<std::shared_ptr<VarBase_t>>&>(props);
                
                //IndexScopeHandler handler{state._current_index};
                if(vector != obj.cache) {
                    std::vector<Layout::Ptr> ptrs;
                    /*if (not obj.state) {
                        obj.state = std::make_unique<State>(state);
                    }*/
                    Context tmp = context;
                    for(auto &v : vector) {
                        auto previous = state._variable_values;
                        tmp.variables["i"] = v;
                        auto ptr = parse_object(obj.child, tmp, state, context.defaults);
                        if(ptr) {
                            update_objects(g, ptr, tmp, state);
                        }
                        ptrs.push_back(ptr);
                        state._variable_values = std::move(previous);
                    }
                    
                    o.to<Layout>()->set_children(ptrs);
                    obj.cache = vector;
                    
                } else {
                    Context tmp = context;
                    for(size_t i=0; i<obj.cache.size(); ++i) {
                        auto previous = state._variable_values;
                        tmp.variables["i"] = obj.cache[i];
                        auto& p = o.to<Layout>()->objects().at(i);
                        if(p) {
                            update_objects(g, p, tmp, state);
                        }
                        //p->parent()->stage()->print(nullptr);
                        state._variable_values = std::move(previous);
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
        
        auto text = path.read_file();
        if(previous != text) {
            previous = text;
        } else
            return;
            
        auto result = load(text);
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
    
    //! clear variable state
    state._variable_values.clear();
    
    context.system_variables().emplace(VarFunc("hovered", [&state = state](const VarProps& props) -> bool {
        if(props.parameters.empty()) {
            if(state._current_object)
                return state._current_object->hovered();
            
        } else if(props.parameters.size() == 1) {
            if(auto it = state._named_entities.find(props.parameters.front());
               it != state._named_entities.end())
            {
                return it->second->hovered();
            }
        }
        return false;
    }));
    
    context.system_variables().emplace(VarFunc("selected", [&state = state](const VarProps& props) -> bool {
        if(props.parameters.empty()) {
            if(state._current_object)
                return state._current_object->selected();
            
        } else if(props.parameters.size() == 1) {
            if(auto it = state._named_entities.find(props.parameters.front());
               it != state._named_entities.end())
            {
                return it->second->selected();
            }
        }
        return false;
    }));
    
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
    previous.clear();
    state = {};
}

void update_tooltips(DrawStructure& graph, State& state) {
    if(not state._settings_tooltip) {
        state._settings_tooltip = Layout::Make<SettingsTooltip>();
    }
    
    Layout::Ptr found = nullptr;
    std::string name{"<null>"};
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto & [key, ptr] : state._text_fields) {
        if(not ptr)
            continue;
        
        ptr->_text->set_clickable(true);
        
        if(ptr->representative()->hovered() && (ptr->representative().ptr.get() == graph.hovered_object() || dynamic_cast<Textfield*>(graph.hovered_object()))) {
            found = ptr->representative();
            //if(ptr->representative().is<Combobox>())
                //ptr->representative().to<Combobox>()-
            name = ptr->_ref.valid() ? ptr->_ref.name() : "<null>";
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
