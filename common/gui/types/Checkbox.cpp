#include "Checkbox.h"

namespace cmn::gui {
    IMPLEMENT(Checkbox::box_size) = Size2(15, 15);
    IMPLEMENT(Checkbox::margin)= float(5);

    void Checkbox::init() {
        set_scroll_enabled(true);
        set_scroll_limits(Rangef(0,0), Rangef(0,0));
        set_clickable(true);
        
        add_event_handler(HOVER, [this](auto) { this->set_dirty(); });
        add_event_handler(MBUTTON, [this](Event e) {
            if(e.mbutton.pressed || e.mbutton.button != 0)
                return;
            
            _settings.checked = not _settings.checked;
            this->set_content_changed(true);
            
            _settings.callback();
        });
        
        _description.set_font(_settings.font);
        _box.set_fillclr(_settings.fill_clr);
        _box.set_lineclr(_settings.line_clr);
        _description.set_color(_settings.text_clr);
        _description.set_txt(_settings.text);
        set_bounds(_settings.bounds);
    }
    
    void Checkbox::update() {
        set_background(_settings.fill_clr.alpha(hovered() ? 150 : 100));
        
        //if(_content_changed)
        {
            begin();
            advance_wrap(_box);
            if(_settings.checked)
                add<Rect>(Box(_box.pos() + Vec2(1, 1), _box.size() - Vec2(2, 2)));
                //advance(new Rect(Bounds(_box.pos() + Vec2(1, 1), _box.size() - Vec2(2, 2)), Black));
            if(!_settings.text.empty())
                advance_wrap(_description);
            end();
            
            _box.set_bounds(Bounds(Vec2(margin,  (Base::default_line_spacing(_settings.font) - box_size.height) * 0.5), box_size));
            _description.set_pos(Vec2(box_size.width + _box.pos().x + 4, 0));
            
            if(!_settings.text.empty()) {
                set_size(Size2(_description.width() + _description.pos().x + margin, Base::default_line_spacing(_settings.font)));
            } else {
                set_size(Size2(margin*2 + box_size.width, Base::default_line_spacing(_settings.font)));
            }
        }
    }
}
