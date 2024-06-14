#include "Context.h"
#include <gui/dyn/State.h>

namespace cmn::gui::dyn {

Context::Context(std::initializer_list<std::variant<std::pair<std::string, std::function<void(Action)>>, std::pair<std::string, std::shared_ptr<VarBase_t>>>> init_list)
{
    for (auto&& item : init_list) {
        if (std::holds_alternative<std::pair<std::string, std::function<void(Action)>>>(item)) {
            auto&& pair = std::get<std::pair<std::string, std::function<void(Action)>>>(item);
            actions[pair.first] = std::move(pair.second);
        }
        else if (std::holds_alternative<std::pair<std::string, std::shared_ptr<VarBase_t>>>(item)) {
            auto&& pair = std::get<std::pair<std::string, std::shared_ptr<VarBase_t>>>(item);
            variables[pair.first] = std::move(pair.second);
        }
    }
}

}
