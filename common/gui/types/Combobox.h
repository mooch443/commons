#pragma once
#include <commons.pc.h>
#include <gui/types/Entangled.h>
#include <gui/GuiTypes.h>
#include <gui/types/Layout.h>
#include <gui/types/Dropdown.h>
#include <gui/GUITaskQueue.h>

namespace cmn::gui {

namespace dyn {
class LabeledField;
}
    
class Combobox : public Entangled {
public:
    ATTRIBUTE_ALIAS(OnSelect_t, std::function<void(ParmName)>)
    GUITaskQueue_t *_gui{nullptr};
    
    struct Settings {
        Bounds bounds = Bounds(0, 0, 100, 33);
        FillClr fill_clr{Drawable::accent_color};
        LineClr line_clr{Black.alpha(200)};
        TextClr text_clr{White};
        Font font = Font(0.75, Align::Center);
        std::string param;
        
        attr::HighlightClr highlight{DarkCyan};
        attr::SizeLimit sizeLimit;
        attr::Postfix postfix;
        attr::Prefix prefix;
        attr::Str content;
        
        OnSelect_t on_select;
    };
    
protected:
    Settings _settings;
    
    HorizontalLayout _layout;
    derived_ptr<Dropdown> _dropdown;
    std::unique_ptr<dyn::LabeledField> _value;
    
public:
    template<typename... Args>
    Combobox(GUITaskQueue_t* gui, Args... args) : _gui(gui)
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
    
    void set(OnSelect_t);
    void set(attr::Font font);
    void set(attr::FillClr clr) override;
    void set(attr::LineClr clr) override;
    void set(attr::TextClr clr);
    void set(attr::HighlightClr clr);
    void set(attr::Str content);
    void set(attr::SizeLimit limit);
    void set(attr::Prefix prefix);
    void set(attr::Postfix postfix);
    void set(ListDims_t dims);
    void set(LabelDims_t dims);
    void set(ListLineClr_t clr);
    void set(ListFillClr_t clr);
    void set(LabelColor_t clr);
    void set(LabelBorderColor_t clr);
    void set(Placeholder_t);
    
    void set(std::function<void()> on_click) {
        if(on_click)
            this->on_click([on_click](auto) { on_click(); });
    }
    void set(ParmName name);
    ParmName parameter() const;
    
    void set_bounds(const Bounds& bds) override;
    void set_pos(const Vec2& p) override;
    void set_size(const Size2& p) override;
    
protected:
    void init();
    void update() override;
    void update_value();
};
    
}
