#pragma once
#include <commons.pc.h>
#include <gui/types/ListItemTypes.h>
#include <gui/types/Layout.h>
#include <misc/Timer.h>
#include <gui/dyn/VarProps.h>
#include <gui/LabeledField.h>
#include <gui/dyn/UnresolvedStringPattern.h>

namespace cmn::gui {
class IMGUIBase;
}

namespace cmn::gui::dyn {

struct State;
class LayoutContext;
struct CurrentObjectHandler;

//#if defined(_MSC_VER) && _MSC_VER <= 1930
    template <class T, class...Ts, template<class...> class Tp>
    constexpr inline bool is_one_of(type_t<Tp<Ts...>>, type_t<T>) {
        return (std::is_same_v<Ts, T> || ...);
    }

    template <typename T, typename Variant>
    concept is_in_variant = is_one_of(type_t<Variant>{}, type_t<T>{});
/*#else
    template <typename T, typename Variant>
    concept is_in_variant = requires (Variant v) {
        {[]<typename K, class...Ts, template<class...> class Tp>(const Tp<Ts...>&) {
            if constexpr((std::same_as<Ts, K> || ...)) {
                return std::true_type{};
            } else
                return std::false_type{};
        }.template operator()<T>(v)} -> std::same_as<std::true_type>;
    };
#endif*/

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

// -----------------------------------------------------------------------------
// A thin wrapper that lets Glaze treat an UnresolvedStringPattern like a plain
// JSON string.  Any incoming JSON string (std::string or std::string_view)
// is transparently converted with pattern::UnresolvedStringPattern::prepare().
// -----------------------------------------------------------------------------
struct PatternValue {
    pattern::UnresolvedStringPattern pat{};

    PatternValue() = default;

    /* implicit */ PatternValue(std::string_view s)
        : pat(pattern::UnresolvedStringPattern::prepare(s)) {}

    /* implicit */ PatternValue(const std::string& s)
        : pat(pattern::UnresolvedStringPattern::prepare(s)) {}

    // Allow seamless use where a pattern is expected
    operator const pattern::UnresolvedStringPattern&() const noexcept { return pat; }
    const pattern::UnresolvedStringPattern& get() const noexcept { return pat; }
};

struct ListContents {
    struct ItemTemplate {
        std::optional<PatternValue> text;
        std::optional<PatternValue> detail;
        std::optional<PatternValue> tooltip;
        std::optional<PatternValue> disabled;
        std::optional<PatternValue> action;
    };
    
    std::string variable;
    ItemTemplate item;
    std::unique_ptr<State> _state;
    std::vector<std::shared_ptr<VarBase_t>> cache;
    std::unordered_map<size_t, std::tuple<std::string, std::function<void()>>> on_select_actions;
};

struct VarCache {
    std::string _var, _value;
    glz::json_t::object_t _obj;
};

/*struct Pattern {
    std::string original;
    std::vector<std::string_view> variables;

    std::string toStr() const;
    static std::string class_name() { return "Pattern"; }
};*/
using Pattern = pattern::UnresolvedStringPattern;

using PatternMapType = std::unordered_map<std::string, Pattern, MultiStringHash, MultiStringEqual>;

struct CustomElement {
    std::string name;
    std::function<Layout::Ptr(LayoutContext&)> create;
    std::function<bool(Layout::Ptr&, const Context&, State&, PatternMapType&)> update;

    CustomElement() = default;
    CustomElement(std::string_view name, auto&& create, auto&& update) : name(std::move(name)), create(std::move(create)), update(std::move(update)) {}
    virtual ~CustomElement() {}
};

struct ManualListContents {
    struct Item {
        pattern::UnresolvedStringPattern detail, tooltip, name;
        std::variant<bool, pattern::UnresolvedStringPattern> disabled_template;
    };
    std::vector<Item> items;
    std::vector<DetailTooltipItem> rendered;
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
    std::shared_ptr<LabeledField> _text_field;
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
    std::optional<Pattern*> get_pattern(size_t hash, const std::string&);
    
    std::shared_ptr<Drawable> named_entity(std::string_view);
    std::optional<std::string_view> cached_variable_value(std::string_view name) const;
    void set_cached_variable_value(std::string_view name, std::string_view value);
    
private:
    std::shared_ptr<HashedObject> register_variant(size_t hash, const Layout::Ptr& o);
    void register_delete_callback(const std::shared_ptr<HashedObject>&, const Layout::Ptr&);
};

}

template<>
struct glz::meta<cmn::gui::dyn::ListContents::ItemTemplate> {
  using T = cmn::gui::dyn::ListContents::ItemTemplate;
  static constexpr auto value = glz::object(
    "text"   , &T::text,
    "detail" , &T::detail,
    "tooltip", &T::tooltip,
    "disabled", &T::disabled,
    "action", &T::action
  );
};

//---------------------------------------------------------------
// PatternValue  ⇄  JSON string
//---------------------------------------------------------------
namespace glz {

// 1.  Tell Glaze this type has fully-custom read & write logic
template <> struct meta<cmn::gui::dyn::PatternValue>
{
    static constexpr bool custom_read  = true;   // use our `from`
    static constexpr bool custom_write = true;   // use our `to`
};

/*-----------  Deserialise  (read)  ----------------------------*/
template <> struct from<JSON, cmn::gui::dyn::PatternValue>
{
    template <auto Opts>
    static void op(cmn::gui::dyn::PatternValue& value, auto&&... args)
    {
        std::string tmp;
        parse<JSON>::op<Opts>(tmp, std::forward<decltype(args)>(args)...);
        value = cmn::gui::dyn::PatternValue{tmp};        // converts to UnresolvedStringPattern
    }
};

/*-----------  Serialise  (write)  -----------------------------*/
template <> struct to<JSON, cmn::gui::dyn::PatternValue>
{
    template <auto Opts>
    static void op(const cmn::gui::dyn::PatternValue& value, auto&&... args) noexcept
    {
        const std::string& tmp = *value.get().original;          // emit the original pattern text
        serialize<JSON>::op<Opts>(tmp, std::forward<decltype(args)>(args)...);
    }
};

} // namespace glz
