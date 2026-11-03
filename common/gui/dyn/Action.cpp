#include "Action.h"
#include <gui/dyn/State.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/ParseText.h>

namespace cmn::gui::dyn {

PreAction PreAction::fromStr(std::string_view str) {
    return create_parse_result<PreAction>(str);
}

std::string PreAction::toStr() const {
    return "PreAction<"+std::string(name)+" parms="+Meta::toStr(parameters)+">";
}

std::string Action::toStr() const {
    return "Action<"+std::string(name)+" parms="+Meta::toStr(parameters)+">";
}

Action PreAction::parse(const Context& context, State& state) const {
    if(not original.empty()
       && original.front() == '{'
       && original.back() == '}')
    {
        // Generated commands already contain resolved parameters.
        auto preaction = PreAction::fromStr(parse_text(original, context, state));
        return Action {
            .name = (std::string)preaction.name,
            .parameters = std::vector<std::string>{ preaction.parameters.begin(), preaction.parameters.end() }
        };
    }

    Action action{
        .name = std::string(parse_text(name, context, state))
    };
    
    action.parameters.reserve(parameters.size());
    for(auto &p : parameters) {
        action.parameters.emplace_back(parse_text(p, context, state));
    }
    
    return action;
}

}
