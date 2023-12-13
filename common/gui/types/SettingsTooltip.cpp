#include "SettingsTooltip.h"
#include <misc/default_settings.h>

namespace gui {

SettingsTooltip::SettingsTooltip(
     Drawable* ptr,
     const sprite::Map* map,
     const GlobalSettings::docs_map_t* docs)
  : Tooltip(ptr, 400), map(map), docs(docs)
{
    if(!map)
        this->map = &GlobalSettings::defaults();
    if(!docs)
        this->docs = &GlobalSettings::docs();
}

void SettingsTooltip::set_parameter(const std::string &name) {
    if(_param == name)
        return;
    
    _param = name;
    set_content_changed(true);
}

void SettingsTooltip::update() {
    if(content_changed()) {
        auto str = "<h3>"+_param+"</h3>";
        auto access = GlobalSettings::access_level(_param);
        if(access > AccessLevelType::PUBLIC) {
            str += " <i>("+std::string(access.name());
            if(!GlobalSettings::defaults().has(_param))
                str += ", non-default";
            str += ")</i>\n";
            
        } else if(!GlobalSettings::defaults().has(_param))
            str += "<i>(non-default)</i>\n";
        
        auto ref = GlobalSettings::get(_param);
        str += "type: " +settings::htmlify(ref.valid() ? (std::string)ref.type_name() : "<invalid>") + "\n";
        if(GlobalSettings::defaults().has(_param)) {
            auto ref = GlobalSettings::defaults().operator[](_param);
            str += "default: " +settings::htmlify(ref.get().valueString()) + "\n";
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
