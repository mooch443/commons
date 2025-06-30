#pragma once
#include <commons.pc.h>
#include <gui/dyn/VarProps.h>
#include <misc/colors.h>
#include <gui/GuiTypes.h>
#include <gui/types/SettingsTooltip.h>

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
    std::weak_ptr<Drawable> _current_object;
    std::unordered_map<std::string, std::weak_ptr<Drawable>, MultiStringHash, MultiStringEqual> _named_entities;
    std::unordered_map<std::string, std::string, MultiStringHash, MultiStringEqual> _variable_values;
    
    std::shared_ptr<SettingsTooltip> _tooltip_object;
    std::vector<std::tuple<std::weak_ptr<LabeledField>, std::weak_ptr<Drawable>>> _textfields;
    
    void reset();
    void select(const std::shared_ptr<Drawable>&);
    void register_named(const std::string& name, const std::shared_ptr<Drawable>& ptr);
    
    void set_variable_value(std::string_view name, std::string_view value);
    std::optional<std::string_view> get_variable_value(std::string_view name) const;
    void remove_variable(std::string_view name);
    
    std::shared_ptr<Drawable> retrieve_named(std::string_view name);
    std::shared_ptr<Drawable> get() const;
    
    void add_tooltip(DrawStructure&);
    void set_tooltip(const std::string_view& parameter, std::weak_ptr<Drawable> other);
    void set_tooltip(std::nullptr_t);
    
    void register_tooltipable(std::weak_ptr<LabeledField>, std::weak_ptr<Drawable>);
    void unregister_tooltipable(std::weak_ptr<LabeledField>);
    void update_tooltips(DrawStructure&);
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

    [[nodiscard]] const std::shared_ptr<VarBase_t>& variable(std::string_view) const;
    [[nodiscard]] bool has(std::string_view) const noexcept;
    [[nodiscard]] std::optional<decltype(variables)::const_iterator> find(std::string_view) const noexcept;
    
    Context() noexcept = default;
    Context(std::initializer_list<std::variant<ActionPair, VariablePair>> init_list);
    
private:
    void init() const;
};

}
