#pragma once

#include <commons.pc.h>
#include <nlohmann/json.hpp>
#include <gui/colors.h>
#include <file/Path.h>
#include <gui/types/Layout.h>
#include <misc/PackLambda.h>
#include <misc/SpriteMap.h>
#include <misc/Timer.h>
#include <gui/GuiTypes.h>
#include <gui/types/Textfield.h>
#include <gui/types/Dropdown.h>
#include <gui/types/Checkbox.h>
#include <gui/types/List.h>
#include <gui/DynamicVariable.h>
#include <gui/LabeledField.h>
#include <variant>

namespace gui {
namespace dyn {

ENUM_CLASS(LayoutType,
           each,
           condition,
           vlayout,
           hlayout,
           gridlayout,
           collection,
           button,
           circle,
           text,
           stext,
           rect,
           textfield,
           checkbox,
           settings,
           combobox,
           image,
           list,
           unknown);

struct VarProps {
    std::string name;
    std::vector<std::string> subs;
    std::vector<std::string> parameters;
    bool optional{false};
    bool html{false};
    
    const std::string& first() const { return parameters.front(); }
    const std::string& last() const { return parameters.back(); }
    
    static VarProps fromStr(std::string);
    std::string toStr() const;
    static std::string class_name() { return "VarProps"; }
    //operator std::string() const { return last(); }
};

struct Context;
struct State;

struct PreVarProps {
    std::string original;
    std::string_view name;
    std::vector<std::string_view> subs;
    std::vector<std::string_view> parameters;
    
    bool optional{false};
    bool html{false};
    
    PreVarProps() noexcept = default;
    
    // Copy constructor
    PreVarProps(const PreVarProps& other)
        : original(other.original), // copy the string
          optional(other.optional),
          html(other.html)
    {
        // Compute the offset in the original string for 'name'
        auto offset_name = other.name.data() - other.original.data();
        name = std::string_view(original.data() + offset_name, other.name.size());

        // Compute the offsets in the original string for 'subs' and 'parameters'
        subs.resize(other.subs.size());
        parameters.resize(other.parameters.size());
        for (size_t i = 0; i < other.subs.size(); ++i) {
            auto offset = other.subs[i].data() - other.original.data();
            subs[i] = std::string_view(original.data() + offset, other.subs[i].size());
        }
        for (size_t i = 0; i < other.parameters.size(); ++i) {
            auto offset = other.parameters[i].data() - other.original.data();
            parameters[i] = std::string_view(original.data() + offset, other.parameters[i].size());
        }
    }
    
    // Move constructor
    PreVarProps(PreVarProps&& other) noexcept {
        original = (other.original);
        optional = other.optional;
        html = other.html;
        
        // Compute the offset in the original string for 'name'
        auto offset_name = other.name.data() - other.original.data();
        name = std::string_view(original.data() + offset_name, other.name.size());

        // Compute the offsets in the original string for 'subs' and 'parameters'
        subs.resize(other.subs.size());
        parameters.resize(other.parameters.size());
        for (size_t i = 0; i < other.subs.size(); ++i) {
            auto offset = other.subs[i].data() - other.original.data();
            subs[i] = std::string_view(original.data() + offset, other.subs[i].size());
        }
        for (size_t i = 0; i < other.parameters.size(); ++i) {
            auto offset = other.parameters[i].data() - other.original.data();
            parameters[i] = std::string_view(original.data() + offset, other.parameters[i].size());
        }
    }
    
    std::string toStr() const;
    static std::string class_name() { return "PreVarProps"; }
    VarProps parse(const Context& context, State& state) const;
};

using VarBase_t = VarBase<const VarProps&>;
using VarReturn_t = sprite::Reference;

struct DefaultSettings {
    Vec2 scale{1.f};
    Vec2 pos{0.f};
    Vec2 origin{0.f};
    Size2 size{0.f};
    Bounds pad{0.f,0.f,0.f,0.f};
    std::string name;
    Color fill{Transparent};
    Color line{Transparent};
    Color verticalClr{DarkCyan.alpha(150)};
    Color horizontalClr{DarkGray.alpha(150)};
    Color highlightClr{Drawable::accent_color};
    Color textClr{White};
    Color window_color{Transparent};
    bool clickable{false};
    Size2 max_size{0.f};
    Font font{0.75f};
    
    std::unordered_map<std::string, std::shared_ptr<VarBase<const Context&, State&>>, MultiStringHash, MultiStringEqual> variables;
};

struct Action {
    std::string name;
    std::vector<std::string> parameters;
    
    const std::string& first() const { return parameters.front(); }
    const std::string& last() const { return parameters.back(); }
    std::string toStr() const;
    static std::string class_name() { return "Action"; }
};

struct PreAction {
    std::string original;
    std::string_view name;
    std::vector<std::string_view> parameters;
    
    PreAction() noexcept = default;

    // Copy constructor
    PreAction(const PreAction& other)
        : original(other.original)
    {
        name = std::string_view(original.data() + (other.name.data() - other.original.data()), other.name.size());
        parameters.reserve(other.parameters.size());
        for (const auto& param : other.parameters) {
            auto offset = param.data() - other.original.data();
            parameters.emplace_back(original.data() + offset, param.size());
        }
    }

    // Move constructor
    PreAction(PreAction&& other) noexcept {
        original = (other.original);
        name = std::string_view(original.data() + (other.name.data() - other.original.data()), other.name.size());
        parameters.reserve(other.parameters.size());
        for (const auto& param : other.parameters) {
            auto offset = param.data() - other.original.data();
            parameters.emplace_back(original.data() + offset, param.size());
        }
    }

    // Copy assignment operator
    PreAction& operator=(const PreAction& other) = delete;

    // Move assignment operator
    PreAction& operator=(PreAction&& other) noexcept = default;
    
    static PreAction fromStr(std::string_view);
    std::string toStr() const;
    static std::string class_name() { return "PreAction"; }
    
    Action parse(const Context& context, State& state) const;
};

template<typename Fn>
std::pair<std::string, std::function<void(Action)>> ActionFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::function<void(Action)>(std::forward<Fn>(func)));
}

template<typename Fn, typename T = typename cmn::detail::return_type<Fn>::type>
std::pair<std::string, std::shared_ptr<VarBase_t>> VarFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::shared_ptr<VarBase_t>(new Variable(std::forward<Fn>(func))));
}

class LayoutContext;

struct Pattern {
    std::string original;
    std::vector<std::string_view> variables;

    std::string toStr() const;
    static std::string class_name() { return "Pattern"; }
};

struct CustomElement {
	std::string name;
	std::function<Layout::Ptr(LayoutContext&)> create;
    std::function<void(Layout::Ptr&, const Context&, State&, const robin_hood::unordered_map<std::string, Pattern>&)> update;
};

struct Context {
    std::unordered_map<std::string, std::function<void(Action)>, MultiStringHash, MultiStringEqual> actions;
    std::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual> variables;
    DefaultSettings defaults;
    
    mutable std::optional<std::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual>> _system_variables;

    std::unordered_map<std::string, CustomElement> custom_elements;
    auto& system_variables() const noexcept {
        if(not _system_variables.has_value())
            init();
        return *_system_variables;
    }

    [[nodiscard]] const std::shared_ptr<VarBase_t>& variable(const std::string_view&) const;
    [[nodiscard]] bool has(const std::string_view&) const noexcept;
    
    Context() noexcept = default;
    Context(std::initializer_list<std::variant<std::pair<std::string, std::function<void(Action)>>, std::pair<std::string, std::shared_ptr<VarBase_t>>>> init_list)
    {
        for (auto&& item : init_list) {
            if (std::holds_alternative<std::pair<std::string, std::function<void(Action)>>>(item)) {
                auto&& pair = std::get<std::pair<std::string, std::function<void(Action)>>>(item);
                actions[pair.first] = std::move(pair.second);
            }
            else if (std::holds_alternative<std::pair<std::string, std::shared_ptr<VarBase_t>>>(item)) {
                auto&& pair = std::get<std::pair<std::string, std::shared_ptr<VarBase_t>>>(item);
                variables[pair.first] = std::move(pair.second);
            }
        }
    }
    
private:
    void init() const;
};

struct State;

struct LoopBody {
    std::string variable;
    nlohmann::json child;
    std::unique_ptr<State> state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
};

struct IfBody {
    std::string variable;
    Layout::Ptr _if;
    Layout::Ptr _else;
};

struct ListContents {
    std::string variable;
    nlohmann::json item;
    std::unique_ptr<State> state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
    std::unordered_map<size_t, std::tuple<std::string, std::function<void()>>> on_select_actions;
};

struct ManualListContents {
    std::vector<DetailItem> items;
};

// Index class manages unique identifiers for objects.
class Index {
public:
    // Global atomic counter used for generating unique IDs.
    uint64_t _global_id{1u};
    
public:
    // Push a new unique ID to the id_chain, representing a new level.
    void push_level() {
    }
    
    // Remove the last ID from the id_chain, effectively stepping out of the current level.
    void pop_level() {
    }
    
    // Increment the global ID counter and update the last ID in id_chain.
    void inc() {
        ++_global_id;
    }
    
    // Returns the last ID in id_chain as the hash.
    // This assumes that the last ID is unique across all instances.
    std::size_t hash() {
        return _global_id++;
    }
};

class IndexScopeHandler {
private:
    Index& index;
public:
    IndexScopeHandler(Index& idx) : index(idx) {
        index.push_level();
    }

    ~IndexScopeHandler() {
        index.pop_level();
    }

    // Disallow copying and moving
    IndexScopeHandler(const IndexScopeHandler&) = delete;
    IndexScopeHandler& operator=(const IndexScopeHandler&) = delete;
};

struct VarCache {
    std::string _var, _value;
    nlohmann::json _obj;
};


struct State {
    robin_hood::unordered_map<size_t, robin_hood::unordered_map<std::string, Pattern>> patterns;
    std::unordered_map<size_t, std::function<void(DrawStructure&)>> display_fns;
    std::unordered_map<size_t, LoopBody> loops;
    std::unordered_map<size_t, ListContents> lists;
    std::unordered_map<size_t, ManualListContents> manual_lists;
    std::unordered_map<size_t, IfBody> ifs;
    
    std::unordered_map<size_t, std::unique_ptr<LabeledField>> _text_fields;
    Layout::Ptr _settings_tooltip;
    std::unordered_map<std::string, std::tuple<size_t, Image::Ptr>> _image_cache;
    std::unordered_map<size_t, VarCache> _var_cache;
    std::unordered_map<size_t, Timer> _timers;
    
    std::unordered_map<std::string, std::string, MultiStringHash, MultiStringEqual> _variable_values;
    std::unordered_map<size_t, std::string> _customs;
    std::unordered_map<size_t, Layout::Ptr> _customs_cache;
    std::unordered_map<std::string, Layout::Ptr, MultiStringHash, MultiStringEqual> _named_entities;
    
    Drawable* _current_object{nullptr};
    
    Index _current_index;
    
    State() = default;
    State(State&&) = default;
    State(const State& other)
        : patterns(other.patterns),
          display_fns(other.display_fns),
          ifs(other.ifs),
          _var_cache(other._var_cache),
          _customs(other._customs),
          _named_entities(other._named_entities),
          _current_object(other._current_object),
          _current_index(other._current_index)
    {
        for(auto &[k, body] : other.loops) {
            loops[k] = {
                .variable = body.variable,
                .child = body.child,
                .state = std::make_unique<State>(*body.state),
                .cache = body.cache
            };
        }
        for(auto &[k, body] : other.lists) {
            lists[k] = {
                .variable = body.variable,
                .item = body.item,
                .state = std::make_unique<State>(*body.state),
                .cache = body.cache,
                .on_select_actions = {}
            };
        }
        for (auto& [k, body] : other.manual_lists) {
            manual_lists[k] = {
                .items = body.items
            };
        }
    }
    
    State& operator=(const State& other) = default;
    State& operator=(State&& other) = default;
};

namespace Modules {
struct Module {
    std::string _name;
    std::function<void(size_t, State&, const Layout::Ptr&)> _apply;
};

void add(Module&&);
void remove(const std::string& name);
Module* exists(const std::string& name);
}

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state,
                         const DefaultSettings& defaults,
                         uint64_t hash = 0);

std::string parse_text(const std::string_view& pattern, const Context&, State&);

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
        )
      return false;

    return true;
}

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
       && word.front() == '{'
       && word.back() == '}')
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

#if !defined(NDEBUG) && false
#define CTIMER_ENABLED true
#else
#define CTIMER_ENABLED false
#endif
struct CTimer {
#if CTIMER_ENABLED
    struct Timing {
        double count{0};
        uint64_t samples{0};
        
        std::string toStr() const {
            return dec<4>(count / double(samples) * 1000).toStr()+"ms total = "+ dec<4>(count).toStr()+"s";
        }
    };
    
    static inline std::unordered_map<std::string, Timing> timings;
    Timer timer;
#endif
    std::string_view _name;
    CTimer(std::string_view name) : _name(name) { }
#if CTIMER_ENABLED
    ~CTimer() {
        auto elapsed = timer.elapsed();
        auto &field = timings[std::string(_name)];
        field.count += elapsed;
        field.samples++;
        
        if(field.samples % 10000 == 0) {
            print("! [", _name, "] ", field);
            field.count /= double(field.samples);
            field.samples = 1;
        }
    }
#endif
};

bool apply_modifier_to_object(std::string_view name, const Layout::Ptr& object, const Action& value);
std::optional<std::string> get_modifier_from_object(Drawable* object, const VarProps& value);

template<typename ApplyF, typename ErrorF, typename Result = typename cmn::detail::return_type<ApplyF>::type>
inline auto resolve_variable(const std::string_view& word, const Context& context, State& state, ApplyF&& apply, ErrorF&& error) -> Result {
    auto props = extractControls(word);
    
    try {
        if(context.has(props.name)) {
            CTimer ctimer("normal");
            if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
                return apply(*context.variable(props.name), props.parse(context, state));
            else
                static_assert(std::invocable<ApplyF, VarBase_t&, const VarProps&>);
            
        } else if(props.name == "if") {
            CTimer ctimer("if");
            auto p = props.parse(context, state);
            if(p.parameters.size() >= 2) {
                bool condition = convert_to_bool(p.parameters.at(0));
                //print("Condition ", props.parameters.at(0)," => ", condition);
                if(condition)
                    return Meta::fromStr<Result>(p.parameters.at(1));
                else if(p.parameters.size() == 3)
                    return Meta::fromStr<Result>(p.parameters.at(2));
            }
            
        } else if(auto it = context.defaults.variables.find(props.name); it != context.defaults.variables.end()) {
            CTimer ctimer("custom var");
            return Meta::fromStr<Result>(it->second->value<std::string>(context, state));
        } else if(auto it = state._named_entities.find(props.name);
                  it != state._named_entities.end()) 
        {
            auto v = get_modifier_from_object(it->second.get(), props.parse(context, state));
            if(v.has_value()) {
                return Meta::fromStr<Result>(v.value());
            }
        }
        
    } catch(...) {
        // catch exceptions
    }
    
    if constexpr(std::invocable<ErrorF, bool>)
        return error(props.optional);
    else
        return error();
}


tl::expected<std::tuple<DefaultSettings, nlohmann::json>, const char*> load(const file::Path& path);

//! A simple struct that manages properties like the path to where the dyn gui is loaded from,
//! the current context and state, and the current layout as well as which DrawStructure it is rendered to.
struct DynamicGUI {
    file::Path path;
    DrawStructure* graph{nullptr};
    Context context;
    State state;
    Base* base;
    std::vector<Layout::Ptr> objects;
    Timer last_update;
    bool first_load{true};
    
    void update(Layout* parent, const std::function<void(std::vector<Layout::Ptr>&)>& before_add = nullptr);
    
    operator bool() const;
    void clear();
    
private:
    void reload();
    static bool update_objects(DrawStructure& g, Layout::Ptr& o, const Context& context, State& state);
    [[nodiscard]] static bool update_loops(uint64_t hash, DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state);
    [[nodiscard]] static bool update_lists(uint64_t hash, DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state);
    static bool update_patterns(uint64_t hash, Layout::Ptr& o, const Context& context, State& state);
};

void update_tooltips(DrawStructure&, State&);

}
}
