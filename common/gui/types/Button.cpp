#include "Button.h"
#include <gui/DrawSFBase.h>

namespace gui {

void Button::init() {
    set_clickable(true);
    set_background(_settings.fill_clr, _settings.line_clr);
    set_scroll_enabled(true);
    set_bounds(_settings.bounds);
    set_scroll_limits(Rangef(), Rangef());
    _text.set_txt(_settings.text);
    _text.set_color(_settings.text_clr);
    _text.set_font(_settings.font);//Font(0.75, Style::Regular, Align::Center));
    
    add_event_handler(MBUTTON, [this](Event e) {
        if(!e.mbutton.pressed && _settings.toggleable) {
            _toggled = !_toggled;
            this->set_dirty();
        }
    });
}
    
    void Button::set_font(Font font) {
        _text.set_font(font);
    }

    const Font& Button::font() const {
        return _text.font();
    }
    
    void Button::update() {
        Color clr(_settings.fill_clr);

        if(pressed()) {
            clr = clr.exposure(0.3);
            
        } else {
            if(_settings.toggleable && toggled()) {
                if(hovered()) {
                    clr = clr.exposure(0.7);
                } else
                    clr = clr.exposure(0.3);
                
            } else if(hovered()) {
                clr = clr.exposure(1.5);
                clr.a = saturate(clr.a * 1.5);
            }
        }
        
        set_background(clr, _settings.line_clr);
        
        Vec2 offset = pressed() ? Vec2(0.5) : Vec2(0);
        if(_settings.font.align == Align::Center) {
            _text.set_pos(size() * 0.5 + offset);
        } else if(_settings.font.align == Align::Left) {
            _text.set_pos(Vec2(10, height() * 0.5 - _text.height() * 0.5) + offset);
        } else if(_settings.font.align == Align::Right) {
            _text.set_pos(Vec2(width() - 10, height() * 0.5 - _text.height() * 0.5) + offset);
        }
        
        if(content_changed()) {
            begin();
            advance_wrap(_text);
            end();
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
}
