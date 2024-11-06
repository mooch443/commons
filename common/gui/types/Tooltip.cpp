#include "Tooltip.h"
#include <gui/DrawStructure.h>

namespace cmn::gui {
    Tooltip::Tooltip(std::nullptr_t, float max_width) : Tooltip(std::weak_ptr<Drawable>{}, max_width) { }
    Tooltip::Tooltip(std::weak_ptr<Drawable> other, float max_width)
          : _other(std::move(other)),
            _max_width(max_width)
    {
        auto lock = _other.lock();
        _text.create(
            Str{},
            SizeLimit(max_width > 0 || !lock
                ? max_width
                : lock->width(), -1),
            Font(0.6),
            FillClr{Black.alpha(220)},
            LineClr{100,175,250,200},
            Margins{10,10,10,10}
        );
        set_origin(Vec2(0, 1));
        _text.set_clickable(false);
        set_z_index(15);
    }

    void Tooltip::set_other(std::weak_ptr<Drawable> other) {
        auto lock = _other.lock();
        auto lock2 = other.lock();
        
        if(lock == lock2)
            return;
        
        _other = other;
        set_content_changed(true);
    }
    
    void Tooltip::update() {
        if(not stage())
            return;
        
        auto mp = stage()->mouse_position();
        if(parent()) {
            auto tf = parent()->global_transform().getInverse();
            mp = tf.transformPoint(mp) + Vec2(5, 0);
        }
        
        auto window = stage()->dialog_window_size();
        auto ox = 0_F;
        if(stage() && mp.x > window.width * 0.5) {
            ox = 1;
        }

        if(mp.y < window.height * 0.5)
            set_origin(Vec2(ox, 0));
        else
            set_origin(Vec2(ox, 1));
        
        auto o = origin();
        auto text_dims = _text.size();
        if(stage())
            text_dims = text_dims.div(stage()->scale());
        text_dims += Vec2(10);
        
        if(o.y == 0
           && mp.y + text_dims.height > window.height)
        {
            mp.y = window.height - text_dims.height;
            
        } else if(o.y == 1
                  && mp.y - text_dims.height < 0)
        {
            mp.y = text_dims.height;
        }
        
        if(o.x == 0
           && mp.x + text_dims.width > window.width)
        {
            mp.x = window.width - text_dims.width;
            
        } else if(o.x == 1
                  && mp.x - text_dims.width < 10)
        {
            mp.x = text_dims.width + 10;
        }
        
        if(!content_changed()) {
            set_pos(mp); /// only set position here and exist
            return;
        }
        
        auto ctx = OpenContext();
        advance_wrap(_text);
        
        /// set both position and size
        set_bounds(Bounds(mp, _text.size() + Vec2(5, 2) * 2));
    }
    
    void Tooltip::set_text(const std::string& text) {
        if(_text.text() == text)
            return;
        
        _text.set_txt(text);
        set_content_changed(true);
    }
}
