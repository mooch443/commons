#pragma once
#include <commons.pc.h>
#include <gui/dyn/VarProps.h>
#include <gui/colors.h>
#include <misc/vec2.h>
#include <gui/GuiTypes.h>

namespace gui::dyn {

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
    Color highlightClr{Drawable::accent_color};
    Color textClr{White};
    Color window_color{Transparent};
    bool clickable{false};
    Size2 max_size{0.f};
    Font font{0.75f};
    
    std::unordered_map<std::string, std::shared_ptr<VarBase<const Context&, State&>>, MultiStringHash, MultiStringEqual> variables;
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
    Context(std::initializer_list<std::variant<std::pair<std::string, std::function<void(Action)>>, std::pair<std::string, std::shared_ptr<VarBase_t>>>> init_list);
    
private:
    void init() const;
};

}
