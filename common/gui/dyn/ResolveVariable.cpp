#include "ResolveVariable.h"
#include <gui/types/Drawable.h>

namespace gui::dyn {

using namespace cmn;

std::optional<std::string> get_modifier_from_object(Drawable* object, const VarProps& value) {
    std::unordered_map<std::string_view, std::function<std::optional<std::string>(Drawable*)>, MultiStringHash, MultiStringEqual> applicators {
        { "pos",
            [](Drawable* ptr) -> std::optional<std::string> {
                return Meta::toStr(ptr->global_bounds().pos());
            }
        },
        { "fill",
            [](Drawable* ptr) -> std::optional<std::string> {
                if(ptr->type() == Type::ENTANGLED || ptr->type() == Type::SECTION)
                    return Meta::toStr(static_cast<SectionInterface*>(ptr)->bg_fill_color());
                return std::nullopt;
            }
        }
    };
    
    if(auto it = applicators.find(value.parameters.front());
       it != applicators.end())
    {
        return it->second(object);
    }
    
    return std::nullopt;
}

}
