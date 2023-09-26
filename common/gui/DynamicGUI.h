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
           image);

inline Color parse_color(const auto& obj) {
    if(not obj.is_array())
        throw std::invalid_argument("Color is not an array.");
    if(obj.size() < 3)
        throw std::invalid_argument("Color is not an array of RGB.");
    uint8_t a = 255;
    if(obj.size() >= 4) {
        a = obj[3].template get<uint8_t>();
    }
    return Color(obj[0].template get<uint8_t>(),
                 obj[1].template get<uint8_t>(),
                 obj[2].template get<uint8_t>(),
                 a);
}

using VarBase_t = VarBase<std::string>;
using VarReturn_t = sprite::Reference;

struct Context {
    std::unordered_map<std::string, std::function<void(Event)>> actions;
    std::unordered_map<std::string, std::shared_ptr<VarBase_t>> variables;
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

// Index class manages unique identifiers for objects.
class Index {
public:
    // A chain of unique IDs (uint64_t) representing hierarchical levels.
    std::vector<uint64_t> id_chain{ 1u };

    // Global atomic counter used for generating unique IDs.
    inline static std::atomic<uint64_t> _global_id{1u};
    
public:
    // Push a new unique ID to the id_chain, representing a new level.
    void push_level() {
        id_chain.push_back(++_global_id);
    }
    
    // Remove the last ID from the id_chain, effectively stepping out of the current level.
    void pop_level() {
        if(id_chain.empty()) {
            throw std::invalid_argument("id_chain is empty.");
        }
        id_chain.pop_back();
    }
    
    // Increment the global ID counter and update the last ID in id_chain.
    void inc() {
        assert(!id_chain.empty());
        ++_global_id;
        id_chain.back() = _global_id;
    }
    
    // Returns the last ID in id_chain as the hash.
    // This assumes that the last ID is unique across all instances.
    std::size_t hash() const {
        return id_chain.back();
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
          ifs(other.ifs)
    {
        for(auto &[k, body] : other.loops) {
            loops[k] = {
                .variable = body.variable,
                .child = body.child,
                .state = std::make_unique<State>(),//*body.state),
                .cache = body.cache
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
                         State& state);

std::string parse_text(const std::string& pattern, const Context& context);

struct VarProps {
    std::vector<std::string> parts;
    std::string sub;
    bool optional{false};
    bool html{false};
};

std::string extractControls(std::string& variable);
std::string regExtractControls(std::string& variable);

template<typename ApplyF, typename ErrorF>
inline auto resolve_variable(const std::string& word, const Context& context, ApplyF&& apply, ErrorF&& error) -> typename cmn::detail::return_type<ApplyF>::type {
    auto variable = utils::lowercase(word);
    VarProps props;
    
    // Call to the separated function
    std::string controls = extractControls(variable);

    if (controls.find('.') == 0) {
        // optional variable
        props.optional = true;
    }
    if(controls.find('#') == 0) {
        props.html = true;
    }
    
    props.parts = utils::split(variable, ':');
    variable = props.parts.front();
    
    std::string modifiers;
    if(props.parts.size() > 1)
        modifiers = props.parts.back();
    props.parts.erase(props.parts.begin());
    props.sub = modifiers;
    
    if(context.variables.contains(variable)) {
        if constexpr(std::invocable<ApplyF, VarBase_t&, const std::string&>)
            return apply(*context.variables.at(variable), modifiers);
        else if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
            return apply(*context.variables.at(variable), props);
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
        throw std::invalid_argument("Invalid variable type for "+word);
    
    if(word.length() > 2
       && word.front() == '{'
       && word.back() == '}') {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, [&](const VarBase_t& variable, const std::string& modifiers) -> T
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
                auto& tmp = variable.value<sprite::Map&>("null");
                auto ref = tmp[modifiers];
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
            auto& tmp = variable.value<sprite::Map&>("null");
            auto ref = tmp[modifiers];
            if(ref.template is_type<T>()) {
                return ref.get().template value<T>();
            }
            
        } else if(variable.is<VarReturn_t>()) {
            auto tmp = variable.value<VarReturn_t>(modifiers);
            if(tmp.template is_type<T>()) {
                return tmp.get().template value<T>();
            }
        }
        
        throw std::invalid_argument("Invalid variable type for "+word);
        
    }, [&]() -> T {
        throw std::invalid_argument("Invalid variable type for "+word);
    });
}

void update_objects(DrawStructure&, const Layout::Ptr& o, const Context& context, State& state);

inline tl::expected<nlohmann::json, const char*> load(const file::Path& path){
    static Timer timer;
    if(timer.elapsed() < 0.15) {
        return tl::unexpected("Have to wait longer to reload.");
    }
    timer.reset();
    
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        return nlohmann::json::parse(text)["objects"];
    } else
        return tl::unexpected("Nothing changed.");
}

void update_layout(const file::Path&, Context&, State&, std::vector<Layout::Ptr>&);
void update_tooltips(DrawStructure&, State&);

}
}
