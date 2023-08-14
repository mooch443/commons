#include "Tooltip.h"
#include <gui/DrawStructure.h>

namespace gui {
    Tooltip::Tooltip(Drawable* other, float max_width)
          : _other(other),
            _text(SizeLimit(max_width > 0 || !_other
                            ? max_width
                            : _other->width(), -1),
                Font(0.7),
                FillClr{Black.alpha(150)}
            ),
            _max_width(max_width)
    {
        set_text("");
        set_origin(Vec2(0, 1));
        _text.set_clickable(false);
        set_z_index(2);
    }

    void Tooltip::set_other(Drawable* other) {
        if(other == _other)
            return;
        
        _other = other;
        _text.set_size(Size2(_max_width > 0 || !_other ? _max_width : _other->width(), -1));
        set_content_changed(true);
    }
    
    void Tooltip::update() {
        if(not stage())
            return;
        
        auto mp = stage()->mouse_position();
        //auto hovered = stage()->hovered_object();
        /*if(hovered && hovered->type() != Type::ENTANGLED && hovered->parent())
            hovered = hovered->parent();
        if(hovered && dynamic_cast<const Dropdown*>(hovered->parent()) != nullptr)
            hovered = hovered->parent();*/
        
        if(parent()) {
            auto tf = parent()->global_transform().getInverse();
            mp = tf.transformPoint(mp) + Vec2(5, 0);
        }
        
        float ox = 0;
        if(stage() && mp.x > stage()->dialog_window_size().width * 0.5) {
            ox = 1;
        }
            
        if(mp.y - _text.height() < 0)
            set_origin(Vec2(ox, 0));
        else
            set_origin(Vec2(ox, 1));
        
        if(!content_changed()) {
            set_pos(mp);
            return;
        }
        
        begin();
        advance_wrap(_text);
        end();
        
        _text.set_content_changed(true);
        _text.update();
        _text.set_bounds_changed();
        
        set_bounds(Bounds(mp, _text.size() + Vec2(5, 2) * 2));
    }
    
    void Tooltip::set_text(const std::string& text) {
        if(_text.text() == text)
            return;
        
        _text.set_txt(text);
        set_content_changed(true);
    }
}
