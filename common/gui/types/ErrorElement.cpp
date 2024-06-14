#include "ErrorElement.h"

namespace cmn::gui {

void ErrorElement::init() {
    _text = std::make_shared<Text>(Str{"ERROR: element "+name()+" cannot be loaded."});
    set_bounds(_settings.bounds);
}

void ErrorElement::update() {
    if(not content_changed())
        return;
    
    set_background(_settings.fill_clr, _settings.line_clr);
    _text->set_txt(_settings.content);
    _text->set_font(_settings.font);
    //auto_size();
    
    begin();
    advance_wrap(*_text);
    end();
}

void ErrorElement::set(attr::Str content) {
    if(_settings.content != content) {
        _settings.content = content;
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::FillClr clr) {
    if(_settings.fill_clr != clr) {
        _settings.fill_clr = clr;
        Entangled::set(clr);
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::LineClr clr) {
    if(_settings.line_clr != clr) {
        _settings.line_clr = clr;
        Entangled::set(clr);
        set_content_changed(true);
    }
}
void ErrorElement::set(attr::TextClr clr) {
    if(_settings.text_clr != clr) {
        _settings.text_clr = clr;
        if(_text)
            _text->set(clr);
        set_content_changed(true);
    }
}

void ErrorElement::set_bounds(const Bounds& bds) {
    if(not _settings.bounds.Equals(bds)) {
        _settings.bounds = bds;
        set_content_changed(true);
    }
    Entangled::set_bounds(bds);
}

void ErrorElement::set_pos(const Vec2& p) {
    if(not _settings.bounds.pos().Equals(p)) {
        _settings.bounds << p;
        set_content_changed(true);
    }
    Entangled::set_pos(p);
}

void ErrorElement::set_size(const Size2& p) {
    if(not _settings.bounds.size().Equals(p)) {
        _settings.bounds << p;
        set_content_changed(true);
    }
    Entangled::set_size(p);
}

}
