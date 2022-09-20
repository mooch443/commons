#ifndef _BUTTON_H
#define _BUTTON_H

#include <gui/types/Drawable.h>
#include <gui/types/Basic.h>
#include <gui/GuiTypes.h>
#include <gui/DrawStructure.h>
#include <gui/ControlsAttributes.h>

namespace gui {
    class Button : public Entangled {
    public:
        struct Settings {
            std::string text;
            Bounds bounds = Bounds(0, 0, 100, 33);
            Color fill_clr = Drawable::accent_color;
            Color line_clr = Black.alpha(200);
            Color text_clr = White;
            bool toggleable = false;
            Font font;
        };
        
    protected:
        Settings _settings;
        GETTER_I(bool, toggled, false)
        Text _text;
        
    public:
        template<typename... Args>
        Button(Args... args)
        {
            create(std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void create(Args... args) {
            (set(std::forward<Args>(args)), ...);
            init();
        }
        
    private:
        void init();
        
        void set(attr::Font font)   { _settings.font = font; }
        void set(attr::Loc loc)     { _settings.bounds << loc; Drawable::set(loc); }
        void set(attr::FillClr clr) { _settings.fill_clr = clr; }
        void set(attr::LineClr clr) { _settings.line_clr = clr; }
        void set(attr::TextClr clr) { _settings.text_clr = clr; }
        void set(attr::Size size)   { _settings.bounds << size; Drawable::set(size); }
        void set(Bounds bounds)   { _settings.bounds = bounds; Drawable::set(bounds); }
        void set(const std::string& text) { _settings.text = text; }
        void set(std::function<void()> on_click) {
            if(on_click)
                this->on_click([on_click](auto) { on_click(); });
        }
        
    public:
        //Button();
        //Button(Settings, std::function<void()> on_click = nullptr);
        virtual ~Button() {}
        
        template<typename... Args>
        static std::shared_ptr<Button> MakePtr(Args... args)
        {
            return std::make_shared<Button>(std::forward<Args>(args)...);
        }
        
        const std::string& txt() const { return _settings.text; }
        void set_txt(const std::string& txt);
        void set_font(Font font);
        const Font& font() const;
        
        void set_text_clr(const Color & text_clr) {
            if ( _settings.text_clr == text_clr )
                return;
            
            _settings.text_clr = text_clr;
            _text.set_color(text_clr);
            set_dirty();
        }
        
        void set_fill_clr(const Color& fill_clr);
        void set_line_clr(const Color& line_clr);
        
        void update() override;
        void set_toggleable(bool v);
        void set_toggle(bool v);
    };
}

#endif
