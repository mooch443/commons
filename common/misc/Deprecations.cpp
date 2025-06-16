#include "Deprecations.h"

namespace cmn {

bool Deprecations::is_deprecated(std::string_view name) const
{
    auto lower = utils::lowercase(name);
    return deprecations.find(lower) != deprecations.end();
}

bool Deprecations::correct_deprecation(std::string_view name, std::string_view val, const sprite::Map *additional, sprite::Map &output) const
{
    auto lower = utils::lowercase(name);
    auto it = deprecations.find(lower);
    if(it == deprecations.end())
        return false;
    
    if(auto& resolution = it->second.replacement;
       not resolution.has_value() || resolution->empty())
    {
        FormatWarning("Parameter ", lower, " is deprecated and has no replacement.", it->second.has_note() ? " Deprecation note: "+it->second.note.value() : "");
        
    } else {
        if(not output.has(*resolution)
           && additional
           && additional->has(*resolution))
        {
            additional->at(*resolution).get().copy_to(output);
        }
        
        if(it->second.has_apply()) {
            it->second.apply_fn(it->second, val, output);
        } else {
            auto& obj = output[*resolution].get();
            obj.set_value_from_string((std::string)val);
        }
    }
    
    return true;
}


}
