#pragma once
#include <commons.pc.h>
#include <gui/types/Entangled.h>
#include <gui/GuiTypes.h>
#include <gui/types/Layout.h>
#include <gui/types/Dropdown.h>

namespace gui {

namespace dyn {
struct LabeledField;
}
    
class Combobox : public Entangled {
public:
    struct Settings {
        Bounds bounds = Bounds(0, 0, 100, 33);
        FillClr fill_clr = Drawable::accent_color;
        LineClr line_clr = Black.alpha(200);
        TextClr text_clr = White;
        Font font = Font(0.75, Align::Center);
        std::string param;
        
        attr::HighlightClr highlight{DarkCyan};
        attr::SizeLimit sizeLimit;
        attr::Postfix postfix;
        attr::Prefix prefix;
        attr::Content content;
    };
    
protected:
    Settings _settings;
    
    HorizontalLayout _layout;
    derived_ptr<Dropdown> _dropdown;
    std::unique_ptr<dyn::LabeledField> _value;
    
public:
    template<typename... Args>
    Combobox(Args... args)
    {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
public:
    using Entangled::set;
    
    void set(attr::Font font);
    void set(attr::FillClr clr) override;
    void set(attr::LineClr clr) override;
    void set(attr::TextClr clr);
    void set(attr::HighlightClr clr);
    void set(attr::Content content);
    void set(attr::SizeLimit limit);
    void set(attr::Prefix prefix);
    void set(attr::Postfix postfix);
    
    void set(std::function<void()> on_click) {
        if(on_click)
            this->on_click([on_click](auto) { on_click(); });
    }
    void set(ParmName name);
    
    void set_bounds(const Bounds& bds) override;
    void set_pos(const Vec2& p) override;
    void set_size(const Size2& p) override;
    
protected:
    void init();
    void update() override;
};
    
}