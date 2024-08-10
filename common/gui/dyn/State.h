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
struct CurrentObjectHandler;

#if defined(_MSC_VER) && _MSC_VER <= 1930
    template <class T> struct type {};
    template <class T> constexpr type<T> type_v{};

    template <class T, class...Ts, template<class...> class Tp>
    constexpr inline bool is_one_of(type<Tp<Ts...>>, type<T>) {
        return (std::is_same_v<Ts, T> || ...);
    }

    template <typename T, typename Variant>
    concept is_in_variant = is_one_of(type<Variant>{}, type<T>{});
#else
    template <typename T, typename Variant>
    concept is_in_variant = requires (Variant v) {
        {[]<typename K, class...Ts, template<class...> class Tp>(const Tp<Ts...>&) {
            if constexpr((std::same_as<Ts, K> || ...)) {
                return std::true_type{};
            } else
                return std::false_type{};
        }.template operator()<T>(v)} -> std::same_as<std::true_type>;
    };
#endif

struct LoopBody {
    std::string variable;
    glz::json_t::object_t child;
    std::unique_ptr<State> _state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
};

struct IfBody {
    std::string variable;
    glz::json_t __if, __else;
    Layout::Ptr _if;
    Layout::Ptr _else;
    
    size_t _assigned_hash{0};
    std::unique_ptr<State> _state;
    
    //~IfBody();
};

struct ListContents {
    std::string variable;
    glz::json_t::object_t item;
    std::unique_ptr<State> _state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
    std::unordered_map<size_t, std::tuple<std::string, std::function<void()>>> on_select_actions;
};

struct VarCache {
    std::string _var, _value;
    glz::json_t::object_t _obj;
};

struct Pattern {
    std::string original;
    std::vector<std::string_view> variables;

    std::string toStr() const;
    static std::string class_name() { return "Pattern"; }
};

using PatternMapType = std::unordered_map<std::string, Pattern, MultiStringHash, MultiStringEqual>;

struct CustomElement {
    std::string name;
    std::function<Layout::Ptr(LayoutContext&)> create;
    std::function<bool(Layout::Ptr&, const Context&, State&, const PatternMapType&)> update;
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

struct CustomBody {
    std::string name;
    Layout::Ptr _customs_cache;
};

struct HashedObject {
    using VariantType = std::variant<std::monostate, IfBody, CustomBody, LoopBody, ListContents, ManualListContents>;
    
    size_t hash;
    Timer timer;
    VariantType object;
    
    std::optional<std::function<void(DrawStructure&)>> display_fn;
    std::optional<std::string> chosen_image;
    std::unique_ptr<LabeledField> _text_field;
    std::optional<VarCache> _var_cache;
    PatternMapType patterns;
    Layout::Ptr current;
    
    template<typename T>
        requires (is_in_variant<T, HashedObject::VariantType>)
    void apply_if(auto&& fn) {
        if(not std::holds_alternative<T>(object))
            return;
        
        fn(std::get<T>(object));
    }
    
    template<typename... Ts>
    void run_if_one_of(auto&& fn) {
        // Lambda to apply the function if the type matches
        auto apply = [&](auto&& arg) {
            if constexpr ((std::is_same_v<std::decay_t<decltype(arg)>, Ts> || ...)) {
                fn(arg);
            }
        };

        std::visit(apply, object);
    }
    
    bool update(GUITaskQueue_t*, size_t hash, DrawStructure&, Layout::Ptr&, const Context&, State&);
    bool update_if(GUITaskQueue_t *, uint64_t, DrawStructure&, Layout::Ptr &, const Context &, State &);
    bool update_patterns(GUITaskQueue_t* gui, uint64_t hash, Layout::Ptr &o, const Context &context, State &state);
    bool update_lists(GUITaskQueue_t*, uint64_t hash, DrawStructure &,  const Layout::Ptr &o, const Context &context, State &state);
    bool update_manual_lists(GUITaskQueue_t*, uint64_t hash, DrawStructure &,  const Layout::Ptr &o, const Context &context, State &state);
    bool update_loops(GUITaskQueue_t* gui, uint64_t hash, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state);
};

struct State {
    //robin_hood::unordered_map<size_t, robin_hood::unordered_map<std::string, Pattern>> patterns;
    Layout::Ptr _settings_tooltip;
    std::unordered_map<std::string, std::tuple<size_t, Image::Ptr>> _image_cache;
    
    std::weak_ptr<CurrentObjectHandler> _current_object_handler;
    derived_ptr<Combobox> _last_settings_box;
    bool _settings_was_selected{false};
    Drawable* _mark_for_selection{nullptr};
    
    Index _current_index;
    
    struct Collectors {
        std::unordered_map<size_t, std::shared_ptr<HashedObject>> objects;
        void dealloc(size_t hash);
    };
    
    std::shared_ptr<Collectors> _collectors = std::make_shared<Collectors>();
    
    State() = default;
    State(State&&) = default;
    State(const State& other) = delete;
    
    State& operator=(const State& other) = delete;
    State& operator=(State&& other) = default;
    
    template<typename T>
        requires (is_in_variant<T, HashedObject::VariantType>)
    std::shared_ptr<HashedObject> register_variant(size_t hash, const Layout::Ptr& o, T&& object) {
        auto obj = register_variant(hash, o);
        obj->object = std::move(object);
        return obj;
    }
    
    std::shared_ptr<HashedObject> register_monostate(size_t hash, const Layout::Ptr&);
    std::shared_ptr<HashedObject> get_monostate(size_t hash, const Layout::Ptr&);
    
    void register_pattern(size_t hash, const std::string&, Pattern&&);
    std::optional<const Pattern*> get_pattern(size_t hash, const std::string&);
    
    std::shared_ptr<Drawable> named_entity(std::string_view);
    std::optional<std::string_view> cached_variable_value(std::string_view name) const;
    void set_cached_variable_value(std::string_view name, std::string_view value);
    
private:
    std::shared_ptr<HashedObject> register_variant(size_t hash, const Layout::Ptr& o);
    void register_delete_callback(const std::shared_ptr<HashedObject>&, const Layout::Ptr&);
};

}
