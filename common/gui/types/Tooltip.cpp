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
        
        auto ctx = OpenContext();
        advance_wrap(_text);
        
        set_bounds(Bounds(mp, _text.size() + Vec2(5, 2) * 2));
    }
    
    void Tooltip::set_text(const std::string& text) {
        if(_text.text() == text)
            return;
        
        _text.set_txt(text);
        set_content_changed(true);
    }
}
