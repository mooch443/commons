#pragma once

#include <gui/types/Entangled.h>
#include <gui/DrawSFBase.h>

namespace gui {
class Checkbox : public Entangled {
public:
    struct Settings {
        std::string text;
        Bounds bounds = Bounds(0, 0, 100, 33);
        Color fill_clr = Drawable::accent_color;
        Color line_clr = Black.alpha(200);
        Color text_clr = White;
        bool checked = false;
        Font font = Font(0.5);
        std::function<void()> callback = [](){};
    };
    
protected:
    Settings _settings;
    
    static const Size2 box_size;
    static const float margin;
    
    Rect _box;
    Text _description;
    
public:
    template<typename... Args>
    Checkbox(Args... args)
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
    void set(attr::Font font)   { set_font(font); }
    void set(attr::FillClr clr) override {
        if(clr != _settings.fill_clr) {
            _settings.fill_clr = clr;
            _box.set_fillclr(clr);
            set_content_changed(true);
        }
    }
    void set(attr::LineClr clr) override {
        if(clr != _settings.line_clr) {
            _settings.line_clr = clr;
            _box.set_lineclr(clr);
            set_content_changed(true);
        }
    }
    void set(attr::TextClr clr) {
        if(_settings.text_clr != clr) {
            _settings.text_clr = clr;
            _description.set_color(clr);
            set_content_changed(true);
        }
    }
    void set(attr::Checked checked) { set_checked(checked); }
    void set(const std::string& text) { set_text(text); }
    void set(std::function<void()> on_change) {
        if(on_change)
            this->on_change([on_change]() { on_change(); });
    }
    
    void on_change(const decltype(Settings::callback)& callback) {
        _settings.callback = callback;
    }
    
    void set_checked(bool checked) {
        if(checked == _settings.checked)
            return;
        
        _settings.checked = checked;
        set_content_changed(true);
    }
    
    bool checked() const { return _settings.checked; }
    
    void set_font(const Font& font) {
        if(_settings.font == font)
            return;
        
        _settings.font = font;
        _description.set_font(font);
        set_content_changed(true);
    }
    
    void set_text(const std::string& text) {
        if(_settings.text == text)
            return;
        
        _settings.text = text;
        _description.set_txt(_settings.text);
        
        set_content_changed(true);
    }
    
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
    
protected:
    void update() override;
};
}
