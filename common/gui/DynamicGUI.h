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
           list);

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
};

struct VarProps {
    std::string name;
    std::vector<std::string> subs;
    std::vector<std::string> parameters;
    bool optional{false};
    bool html{false};
    
    std::string first() const { return parameters.empty() ? "" : parameters.front(); }
    std::string last() const { return parameters.empty() ? "" : parameters.back(); }
    
    static VarProps fromStr(std::string);
    std::string toStr() const;
    static std::string class_name() { return "VarProps"; }
    //operator std::string() const { return last(); }
};

using VarBase_t = VarBase<VarProps>;
using VarReturn_t = sprite::Reference;

struct Action {
    std::string name;
    std::vector<std::string> parameters;
    
    static Action fromStr(std::string_view);
    std::string toStr() const;
    static std::string class_name() { return "Action"; }
};

template<typename Fn>
std::pair<std::string, std::function<void(Action)>> ActionFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::function<void(Action)>(std::forward<Fn>(func)));
}

template<typename Fn, typename T = typename cmn::detail::return_type<Fn>::type>
std::pair<std::string, std::shared_ptr<VarBase_t>> VarFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::shared_ptr<VarBase_t>(new Variable(std::forward<Fn>(func))));
}

struct Context {
    std::unordered_map<std::string, std::function<void(Action)>, MultiStringHash, MultiStringEqual> actions;
    std::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual> variables;
    DefaultSettings defaults;
    
    mutable std::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual> system_variables;
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

struct State {
    robin_hood::unordered_map<size_t, robin_hood::unordered_map<std::string, std::string>> patterns;
    std::unordered_map<size_t, std::function<void(DrawStructure&)>> display_fns;
    std::unordered_map<size_t, LoopBody> loops;
    std::unordered_map<size_t, ListContents> lists;
    std::unordered_map<size_t, IfBody> ifs;
    
    std::map<std::string, std::unique_ptr<LabeledField>> _text_fields;
    Layout::Ptr _settings_tooltip;
    std::unordered_map<std::string, std::tuple<size_t, Image::Ptr>> _image_cache;
    
    Index _current_index;
    
    State() = default;
    State(State&&) = default;
    State(const State& other)
        : patterns(other.patterns),
          display_fns(other.display_fns),
          ifs(other.ifs),
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
                         const DefaultSettings& defaults);

std::string parse_text(const std::string_view& pattern, const Context& context);


VarProps extractControls(const std::string_view& variable, const Context& context);

template<typename ApplyF, typename ErrorF>
inline auto resolve_variable(const std::string_view& word, const Context& context, ApplyF&& apply, ErrorF&& error) -> typename cmn::detail::return_type<ApplyF>::type {
    auto props = extractControls(word, context);
    
    if(context.has(props.name)) {
        if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
            return apply(*context.variable(props.name), props);
        else
            static_assert(std::invocable<ApplyF, VarBase_t&, const VarProps&>);
    }
    
    if constexpr(std::invocable<ErrorF, bool>)
        return error(props.optional);
    else
        return error();
}

template<typename T>
T resolve_variable_type(std::string word, const Context& context) {
    if(word.empty())
        throw U_EXCEPTION("Invalid variable name (is empty).");
    
    if(word.length() > 2
       && word.front() == '{'
       && word.back() == '}') 
    {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, [&](const VarBase_t& variable, const VarProps& modifiers) -> T
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
    static void update_objects(DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state);
    [[nodiscard]] static bool update_loops(uint64_t hash, DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state);
    [[nodiscard]] static bool update_lists(uint64_t hash, DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state);
};

void update_tooltips(DrawStructure&, State&);

}
}
