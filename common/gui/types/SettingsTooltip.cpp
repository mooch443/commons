#include "SettingsTooltip.h"
#include <gui/types/List.h>
#include <gui/types/Combobox.h>

namespace cmn::gui {

SettingsTooltip::SettingsTooltip(
     std::weak_ptr<Drawable> ptr)
  : Tooltip(ptr, 500)
{
}

void SettingsTooltip::set_parameter(TooltipData name) {
    if(_param == name)
        return;
    
    _param = name;
    set_content_changed(true);
}

void SettingsTooltip::update() {
    if(auto lock = _other.lock();
       lock)
    {
        Drawable* object = lock.get();
        if(auto combo = dynamic_cast<Combobox*>(object);
           combo)
        { /// search for lists, those are interesting
            auto tmp = dynamic_cast<List*>(combo->tooltip_object());
            if(tmp)
                object = tmp;
        }
        
        auto bds = object->global_bounds();
        //bds << bds.pos() + Vec2(bds.size().mul(o));
        
        _param.set_index(std::nullopt);
        
        if(auto ptr = dynamic_cast<List*>(object);
           ptr != nullptr)
        {
            if(ptr->foldable()
               && not ptr->folded())
            {
                /// could be a long list, not part of the bounding box?
                /// calculate, based on current mouse position, the option that we are highlighting in the list. then set it in the tooltipdata as the enum value highlighted. just in case we can show something more interactive.
                auto t = ptr->global_transform();
                //t.translate(ptr->last_list_offset());
                auto offset = ptr->last_list_offset();
                auto row_height = (t.transformPoint(0, ptr->row_height()) - bds.pos()).y;
                
                auto mp = stage()->mouse_position();
                if(parent()) {
                    auto tf = parent()->global_transform().getInverse();
                    mp = tf.transformPoint(mp) + Vec2(5, 0);
                }
                
                if(offset.y < 0) {
                    auto row_index = (bds.pos().y - mp.y) / row_height;
                    if(row_index >= 0
                       && uint64_t(row_index) < ptr->items().size())
                    {
                        _param.set_index(narrow_cast<uint8_t>(int(row_index)));
                    }
                    
                } else {
                    auto row_index = (mp.y - bds.pos().y - bds.height) / row_height;
                    if(row_index >= 0
                       && uint64_t(row_index) < ptr->items().size())
                    {
                        _param.set_index(narrow_cast<uint8_t>(int(row_index)));
                    }
                }
            }
        }
    }
    
    
    if(content_changed()) {
        set_text(_param.text());
    }
    
    if(stage())
        set_scale(stage()->scale().reciprocal());
    
    Tooltip::update();
}

}
