#pragma once
#include <commons.pc.h>
#include <gui/dyn/VarProps.h>
#include <misc/colors.h>
#include <gui/GuiTypes.h>
#include <gui/types/SettingsTooltip.h>
#include <misc/TooltipData.h>

namespace cmn::gui::dyn {

class LabeledField;

struct Action;
struct CustomElement;

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
    Color cellFillClr{Transparent};
    Color cellLineClr{Transparent};
    CornerFlags corners{};
    uint16_t cellFillInterval{1};
    Size2 minCellSize;
    Color highlightClr{Drawable::accent_color};
    Color textClr{White};
    Color window_color{Transparent};
    bool clickable{false};
    Size2 max_size{0.f};
    Font font{0.75f};
    
    std::unordered_map<std::string, std::shared_ptr<VarBase<const Context&, State&>>, MultiStringHash, MultiStringEqual> variables;
};

struct CurrentObjectHandler {
    struct VariableValue {
        enum class Kind : uint8_t {
            none,
            string,
            dynamic
        };

        Kind kind{Kind::none};
        union Storage {
            Storage() {}
            ~Storage() {}

            std::string string_value;
            std::shared_ptr<VarBase_t> dynamic_value;
        } storage;

        VariableValue() = default;
        VariableValue(std::string_view value) { set(value); }
        VariableValue(std::shared_ptr<VarBase_t> value) { set(std::move(value)); }
        VariableValue(const VariableValue& other);
        VariableValue(VariableValue&& other) noexcept;
        VariableValue& operator=(const VariableValue& other);
        VariableValue& operator=(VariableValue&& other) noexcept;
        ~VariableValue();

        void reset() noexcept;
        void set(std::string_view value);
        void set(std::shared_ptr<VarBase_t> value);

        [[nodiscard]] bool is_string() const noexcept { return kind == Kind::string; }
        [[nodiscard]] bool is_dynamic() const noexcept { return kind == Kind::dynamic; }
        [[nodiscard]] std::string_view string() const;
        [[nodiscard]] std::shared_ptr<VarBase_t> dynamic() const;

        bool operator==(const VariableValue& other) const;
    };

    /// Cached parse result with the variable versions it depends on.
    struct CachedVariableValue {
        /// Monotonic version for global `_variable_values`.
        uint64_t global_version{0};
        /// Monotonic version for loop-local scope overlays.
        uint64_t scoped_version{0};
        std::string value;
    };

    struct ScopedVariableSnapshot {
        using ScopeMap = std::unordered_map<std::string, VariableValue, MultiStringHash, MultiStringEqual>;

        std::vector<ScopeMap> values;

        [[nodiscard]] bool empty() const noexcept { return values.empty(); }
    };
    
    /// RAII helper that pushes a loop-local variable scope on creation and pops on destruction.
    class ScopedVariables {
    public:
        explicit ScopedVariables(CurrentObjectHandler& handler);
        ScopedVariables(const ScopedVariables&) = delete;
        ScopedVariables& operator=(const ScopedVariables&) = delete;
        ScopedVariables(ScopedVariables&& other) noexcept;
        ScopedVariables& operator=(ScopedVariables&& other) noexcept;
        ~ScopedVariables();

        /// Set or override a variable in the current local scope.
        template<typename T>
        void set(std::string_view name, T&& value) {
            if(not _handler) {
                return;
            }
            
            _handler->set_scoped_variable_value(name, std::forward<T>(value));
        }
        
    private:
        CurrentObjectHandler* _handler;
        bool _active{true};
        
        void restore();
    };
    
    std::weak_ptr<Drawable> _current_object;
    std::unordered_map<std::string, std::weak_ptr<Drawable>, MultiStringHash, MultiStringEqual> _named_entities;
    std::unordered_map<std::string, VariableValue, MultiStringHash, MultiStringEqual> _variable_values;
    std::vector<std::unordered_map<std::string, VariableValue, MultiStringHash, MultiStringEqual>> _scoped_variable_values;
    std::unordered_map<std::string, CachedVariableValue, MultiStringHash, MultiStringEqual> _cached_variable_values;
    uint64_t _variable_values_version{1};
    uint64_t _scoped_variable_values_version{1};
    
    std::shared_ptr<SettingsTooltip> _tooltip_object;
    std::vector<std::tuple<std::weak_ptr<LabeledField>, std::weak_ptr<Drawable>>> _textfields;
    
    void reset();
    void select(const std::shared_ptr<Drawable>&);
    void register_named(const std::string& name, const std::shared_ptr<Drawable>& ptr);
    
    /// Set a global (non-scoped) variable.
    void set_variable_value(std::string_view name, std::string_view value);
    /// Resolve a variable by checking scoped overlays first, then global values.
    std::optional<std::string_view> get_variable_value(std::string_view name) const;
    /// Resolve a typed variable by checking scoped overlays first, then global values.
    std::shared_ptr<VarBase_t> get_dynamic_variable(std::string_view name) const;
    /// Remove a global (non-scoped) variable.
    void remove_variable(std::string_view name);
    /// Clear global values, scoped overlays, and parse caches.
    void clear_variable_values();
    
    /// Return a cached parse result if both global and scoped versions still match.
    std::optional<std::string_view> get_cached_variable_value(std::string_view name) const;
    /// Store a parse result keyed to the current global and scoped versions.
    void set_cached_variable_value(std::string_view name, std::string_view value);
    /// Capture the current scoped variable stack for later reuse.
    [[nodiscard]] ScopedVariableSnapshot capture_scoped_variable_values() const;
    /// Replace the current scoped variable stack with a previously captured snapshot.
    void restore_scoped_variable_values(const ScopedVariableSnapshot& snapshot);
    /// Replace the current scoped variable stack with a previously captured snapshot.
    void restore_scoped_variable_values(ScopedVariableSnapshot&& snapshot);
    /// Monotonic version for global variable mutations.
    [[nodiscard]] uint64_t variable_values_version() const noexcept { return _variable_values_version; }
    /// Monotonic version for scoped variable mutations, including snapshot restores.
    [[nodiscard]] uint64_t scoped_variable_values_version() const noexcept { return _scoped_variable_values_version; }
    
    /// Create a scoped overlay for temporary loop variables.
    [[nodiscard]] ScopedVariables scope();
    
    std::shared_ptr<Drawable> retrieve_named(std::string_view name);
    std::shared_ptr<Drawable> get() const;
    
    void add_tooltip(DrawStructure&);
    void set_tooltip(TooltipData, std::weak_ptr<Drawable> other);
    void set_tooltip(std::nullptr_t);
    
    void register_tooltipable(std::weak_ptr<LabeledField>, std::weak_ptr<Drawable>);
    void unregister_tooltipable(std::weak_ptr<LabeledField>);
    void update_tooltips(DrawStructure&);
    
private:
    void invalidate_cached_variable_values();
    void invalidate_scoped_cached_variable_values();
    void push_variable_scope();
    void pop_variable_scope();
    void set_scoped_variable_value(std::string_view name, std::string_view value);
    void set_scoped_variable_value(std::string_view name, std::shared_ptr<VarBase_t> value);
    void set_variable_value(std::string_view name, std::shared_ptr<VarBase_t> value);
};

struct Context {
    //using ActionPair = robin_hood::pair<std::string, std::function<void(Action)>>;
    //using VariablePair = robin_hood::pair<std::string, std::function<void(Action)>>;
    robin_hood::unordered_map<std::string, std::function<void(Action)>, MultiStringHash, MultiStringEqual> actions;
    robin_hood::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual> variables;
    
    using ActionPair = decltype(actions)::value_type;
    using VariablePair = decltype(variables)::value_type;
    DefaultSettings defaults;
    
    mutable std::optional<robin_hood::unordered_map<std::string, std::shared_ptr<VarBase_t>, MultiStringHash, MultiStringEqual>> _system_variables;

    std::unordered_map<std::string, std::shared_ptr<CustomElement>> custom_elements;
    Vec2 _last_mouse;
    
    auto& system_variables() const noexcept {
        if(not _system_variables.has_value())
            init();
        return *_system_variables;
    }

    [[nodiscard]] std::shared_ptr<VarBase_t> variable(std::string_view, const State&) const;
    [[nodiscard]] bool has(std::string_view, const State&) const noexcept;
    [[nodiscard]] std::optional<decltype(variables)::const_iterator> find(std::string_view) const noexcept;

    Context() noexcept = default;
    Context(std::initializer_list<std::variant<ActionPair, VariablePair>> init_list);
    
private:
    void init() const;
};

}
