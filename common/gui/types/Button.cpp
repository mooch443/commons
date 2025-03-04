#include "Button.h"
#include <gui/DrawSFBase.h>

namespace cmn::gui {

void Button::init() {
    set_clickable(true);
    set_background(_settings.fill_clr, _settings.line_clr);
    set_scroll_enabled(true);
    set_bounds(_settings.bounds);
    set_scroll_limits(Rangef(), Rangef());
    _text.set_txt(_settings.text);
    _text.set(TextClr{_settings.text_clr});
    _text.set(_settings.font);//Font(0.75, Style::Regular, Align::Center));
    
    add_event_handler(MBUTTON, [this](Event e) {
        if(!e.mbutton.pressed && _settings.toggleable) {
            _toggled = !_toggled;
            this->set_dirty();
        }
    });
}
    
    void Button::set_font(Font font) {
        _text.set(font);
    }

    const Font& Button::font() const {
        return _text.font();
    }
    
    void Button::update() {
        Color clr(_settings.fill_clr);
        Color text_clr(_settings.text_clr);

        if(pressed()) {
            clr = clr.exposure(0.5);
            text_clr = text_clr.exposure(0.5);
            
        } else {
            if(_settings.toggleable && toggled()) {
                if(hovered()) {
                    clr = clr.exposure(0.7);
                    text_clr = text_clr.exposure(0.7);
                } else {
                    clr = clr.exposure(0.5);
                    text_clr = text_clr.exposure(0.5);
                }
                
            } else if(hovered()) {
                clr = clr.exposure(1.5);
                clr.a = saturate(clr.a * 1.5);
                text_clr = text_clr.exposure(1.5);
                text_clr.a = saturate(text_clr.a * 1.5);
            }
        }
        
        set_background(clr, _settings.line_clr);
        
        Vec2 offset = _settings.margins.pos() + (pressed() ? Vec2(0.5) : Vec2(0));
        if(_settings.font.align == Align::Center) {
            _text.set_origin(Vec2(0.5));
            _text.set_pos(size() * 0.5 + offset);
        } else if(_settings.font.align == Align::Left) {
            _text.set_origin(Vec2(0, 0.5));
            _text.set_pos(Vec2(10, height() * 0.5) + offset);
        } else if(_settings.font.align == Align::Right) {
            _text.set_origin(Vec2(1, 0.5));
            _text.set_pos(Vec2(width() - 10, height() * 0.5) + offset);
        } else if(_settings.font.align == Align::VerticalCenter) {
            _text.set_origin(Vec2(0, 0.5));
            _text.set_pos(Vec2(width() - 10, height() * 0.5) + offset);
        }
        _text.set(TextClr{text_clr});
        
        if(content_changed()) {
            auto ctx = OpenContext();
            advance_wrap(_text);
        }
    }
    
    void Button::set_fill_clr(const gui::Color &fill_clr) {
        if(_settings.fill_clr == fill_clr)
            return;
        
        _settings.fill_clr = fill_clr;
        set_dirty();
    }
    
    void Button::set_line_clr(const gui::Color &line_clr) {
        if(_settings.line_clr == line_clr)
            return;
        
        _settings.line_clr = line_clr;
        set_dirty();
    }
    
    void Button::set_txt(const std::string &txt) {
        if(_settings.text == txt)
            return;
        
        _settings.text = txt;
        _text.set_txt(txt);
        set_content_changed(true);
    }
    
    void Button::set_toggleable(bool v) {
        if(_settings.toggleable == v)
            return;
        
        _settings.toggleable = v;
        set_content_changed(true);
    }
    
    void Button::set_toggle(bool v) {
        if(_toggled == v)
            return;
        
        _toggled = v;
        set_content_changed(true);
    }

    Size2 Button::text_dims() {
        return _text.size();
    }
}
