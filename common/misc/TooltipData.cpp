#include "TooltipData.h"
#include <misc/default_settings.h>

namespace cmn::gui {

std::string TooltipData::text() const {
    if(is_name()) {
        std::string name = (std::string)title();
        
        auto str = "<h3>"+name+"</h3>\n";
        auto access = GlobalSettings::access_level(name);
        if(bool has_default = GlobalSettings::has_default(name);
           access > AccessLevelType::PUBLIC)
        {
            str += "access: <i>"+std::string(access.name());
            if(not has_default)
                str += ", non-default";
            str += "</i>\n";
            
        } else if(not has_default)
            str += "<i>non-default</i>\n";
        
        auto ref = GlobalSettings::get(name);
        str += "type: " +settings::htmlify(ref.valid() ? (std::string)ref.type_name() : "<invalid>") + "\n";
        if(auto ref = GlobalSettings::read_default<NoType>(name);
           ref.valid())
        {
            str += "default: " +settings::htmlify(ref->valueString()) + "\n";
        }
        if(auto ref = GlobalSettings::read_example<NoType>(name);
           ref.valid())
        {
            str += "example: " +settings::htmlify(ref->valueString()) + "\n";
        }
        if(auto doc = GlobalSettings::read_doc(name);
           doc)
        {
            str += "\n" + settings::htmlify((std::string)*doc);
        }
        
        return str;
    }
    
    auto str = std::get<Both>(data).name.empty() ? "" : "<h3>"+std::get<Both>(data).name+"</h3>\n";
    return str + (std::string)std::get<Both>(data).docs;
}

std::string TooltipData::toStr() const {
    if(is_name())
        return "{"+Meta::toStr(title())+"}";
    return "{"+Meta::toStr(std::get<Both>(data).name)+","+Meta::toStr(std::get<Both>(data).docs)+"}";
}

}
