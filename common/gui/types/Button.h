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
            Font font = Font(0.75, Align::Center);
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
        
    public:
        using Entangled::set;
        void set(attr::Font font)   { _settings.font = font; }
        void set(attr::Loc loc) override    { _settings.bounds << loc; Entangled::set(loc); }
        void set(attr::FillClr clr) override { _settings.fill_clr = clr; }
        void set(attr::LineClr clr) override { _settings.line_clr = clr; }
        void set(attr::TextClr clr) { _settings.text_clr = clr; }
        void set(attr::Size size) override   { _settings.bounds << size; Entangled::set(size); }
        void set(attr::Box bounds) override   { _settings.bounds = bounds; Entangled::set(bounds); }
        void set(const Str& text) { set_txt(text); }
        void set(std::function<void()> on_click) {
            if(on_click)
                this->on_click([on_click](auto) { on_click(); });
        }
        
    public:
        //Button();
        //Button(Settings, std::function<void()> on_click = nullptr);
        virtual ~Button() {}
        
        using Ptr = std::shared_ptr<Button>;
        
        template<typename... Args>
        static Ptr MakePtr(Args... args)
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
        
        void set_size(const Size2& size) override {
            if(not _settings.bounds.size().Equals(size)) {
                _settings.bounds << size;
                set_content_changed(true);
            }
            Entangled::set_size(attr::Size(size));
        }
        void set_bounds(const Bounds& bounds) override {
            if(not bounds.Equals(_settings.bounds)) {
                _settings.bounds = bounds;
                set_content_changed(true);
            }
            Entangled::set_bounds(bounds);
        }
    };
}

#endif
