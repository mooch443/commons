#include "SettingsTooltip.h"

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
    if(content_changed()) {
        set_text(_param.text());
    }
    
    if(stage())
        set_scale(stage()->scale().reciprocal());
    
    Tooltip::update();
}

}
