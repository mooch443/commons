#include "SettingsTooltip.h"
#include <misc/default_settings.h>

namespace cmn::gui {

SettingsTooltip::SettingsTooltip(
     std::weak_ptr<Drawable> ptr)
  : Tooltip(ptr, 500)
{
}

void SettingsTooltip::set_parameter(const std::string &name) {
    if(_param == name)
        return;
    
    _param = name;
    set_content_changed(true);
}

void SettingsTooltip::update() {
    if(content_changed()) {
        auto str = "<h3>"+_param+"</h3>\n";
        auto access = GlobalSettings::access_level(_param);
        if(access > AccessLevelType::PUBLIC) {
            str += "access: <i>"+std::string(access.name());
            if(!GlobalSettings::get_default(_param))
                str += ", non-default";
            str += "</i>\n";
            
        } else if(!GlobalSettings::defaults().has(_param))
            str += "<i>non-default</i>\n";
        
        auto ref = GlobalSettings::get(_param);
        str += "type: " +settings::htmlify(ref.valid() ? (std::string)ref.type_name() : "<invalid>") + "\n";
        if(auto ref = GlobalSettings::get_default(_param);
           ref)
        {
            str += "default: " +settings::htmlify(ref->valueString()) + "\n";
        }
        if(auto ref = GlobalSettings::config().examples.at(_param);
           ref.valid())
        {
            str += "example: " +settings::htmlify(ref->valueString()) + "\n";
        }
        if(GlobalSettings::has_doc(_param))
            str += "\n" + settings::htmlify(GlobalSettings::doc(_param));
        set_text(str);
    }
    
    if(stage())
        set_scale(stage()->scale().reciprocal());
    
    Tooltip::update();
}

}
