#include "Deprecations.h"

namespace cmn {

bool Deprecations::is_deprecated(std::string_view name) const
{
    auto lower = utils::lowercase(name);
    return deprecations.find(lower) != deprecations.end();
}

std::optional<std::string> Deprecations::replacement(std::string_view key) const {
    const auto it = deprecations.find(utils::lowercase(key));
    return it != deprecations.end() ? it->second.replacement : std::nullopt;
}

void Deprecations::warn_deprecated(std::string_view source, const std::set<std::string>& keys) const {
    bool found = false;
    for(const auto& key : keys) {
        auto it = deprecations.find(utils::lowercase(key));
        if(it == deprecations.end())
            continue;

        found = true;
        const auto& replacement = it->second.replacement;
        if(replacement && not replacement->empty()) {
            FormatExcept("[", source, "] Setting ", key, " is deprecated. Please use its replacement parameter ", *replacement, " instead.");
        } else {
            FormatWarning("[", source, "] Setting ", key, " has been removed (with no replacement) and will be ignored.");
        }
    }
    if(found)
        Print("Found invalid settings in source ", source, " (see above).");
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
