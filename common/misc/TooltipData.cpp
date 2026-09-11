#include "TooltipData.h"
#include <misc/default_settings.h>

namespace cmn::gui {

std::string TooltipData::text() const {
    if(is_name()
       || not std::get<Both>(data).docs)
    {
        std::string name = (std::string)title();
        
        auto str = "<h3>"+name+"</h3>\n";
        
        AccessLevel access;
        bool has_default;
        std::optional<std::string> doc;
        //const Both &both = std::get<Both>(data);
        
        GlobalSettings::read([&](const Configuration& config){
            auto result = config.access_level(name);
            if(result)
                access = result.value();
            else
                access = AccessLevelType::PUBLIC;
            
            has_default = config.get_default(name).has_value();
            
            if(auto it = config.doc_generators.find(name);
               it != config.doc_generators.end())
            {
                if(not is_name())
                    doc = it->second(std::get<Both>(data).enum_offset);
                else
                    doc = it->second(std::nullopt);//both.enum_offset);
            } else {
                if(auto it = config.docs.find(name);
                   it != config.docs.end())
                {
                    doc = it->second;
                }
            }
        });
        //auto access = GlobalSettings::access_level(name);
        if(//bool has_default = GlobalSettings::has_default(name);
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
        if(doc)
        {
            str += "\n" + settings::htmlify((std::string)*doc);
        }
        
        return str;
    }
    
    auto str = std::get<Both>(data).name.empty() ? "" : "<h3>"+std::get<Both>(data).name+"</h3>\n";
    return str + (std::string)std::get<Both>(data).docs.value();
}

std::string TooltipData::toStr() const {
    if(is_name())
        return "{"+Meta::toStr(title())+"}";
    return "{"+Meta::toStr(std::get<Both>(data).name)+","+Meta::toStr(std::get<Both>(data).docs)+"}";
}

void TooltipData::set_index(std::optional<uint8_t> index) {
    if(not is_name()) {
        auto& data = std::get<Both>(this->data);
        data.enum_offset = std::move(index);
    } else if(index.has_value()) {
        auto _name = std::get<std::string>(this->data);
        this->data = Both{
            .name = std::move(_name),
            .docs = std::nullopt,
            .enum_offset = std::move(index)
        };
    }
}

}
