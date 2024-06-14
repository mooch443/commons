#pragma once
#include <commons.pc.h>
#include <gui/types/ListItemTypes.h>
#include <gui/types/Layout.h>
#include <misc/Timer.h>
#include <gui/dyn/VarProps.h>
#include <gui/LabeledField.h>

namespace cmn::gui::dyn {

struct State;
class LayoutContext;

struct LoopBody {
    std::string variable;
    nlohmann::json child;
    //std::unique_ptr<State> state;
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

struct VarCache {
    std::string _var, _value;
    nlohmann::json _obj;
};

struct Pattern {
    std::string original;
    std::vector<std::string_view> variables;

    std::string toStr() const;
    static std::string class_name() { return "Pattern"; }
};

struct CustomElement {
    std::string name;
    std::function<Layout::Ptr(LayoutContext&)> create;
    std::function<bool(Layout::Ptr&, const Context&, State&, const robin_hood::unordered_map<std::string, Pattern>&)> update;
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

struct State {
    robin_hood::unordered_map<size_t, robin_hood::unordered_map<std::string, Pattern>> patterns;
    std::unordered_map<size_t, std::function<void(DrawStructure&)>> display_fns;
    std::unordered_map<size_t, LoopBody> loops;
    std::unordered_map<size_t, ListContents> lists;
    std::unordered_map<size_t, ManualListContents> manual_lists;
    std::unordered_map<size_t, IfBody> ifs;
    std::unordered_map<size_t, std::string> chosen_images;
    
    std::unordered_map<size_t, std::unique_ptr<LabeledField>> _text_fields;
    Layout::Ptr _settings_tooltip;
    std::unordered_map<std::string, std::tuple<size_t, Image::Ptr>> _image_cache;
    std::unordered_map<size_t, VarCache> _var_cache;
    std::unordered_map<size_t, Timer> _timers;
    
    std::unordered_map<std::string, std::string, MultiStringHash, MultiStringEqual> _variable_values;
    std::unordered_map<size_t, std::string> _customs;
    std::unordered_map<size_t, Layout::Ptr> _customs_cache;
    std::unordered_map<std::string, Layout::Ptr, MultiStringHash, MultiStringEqual> _named_entities;
    
    std::weak_ptr<Drawable> _current_object;
    
    Index _current_index;
    
    State() = default;
    State(State&&) = default;
    State(const State& other);
    
    State& operator=(const State& other) = default;
    State& operator=(State&& other) = default;
};

}
