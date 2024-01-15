#pragma once
#include <commons.pc.h>
#include <gui/dyn/ParseText.h>
#include <gui/dyn/VarProps.h>
#include <gui/dyn/State.h>
#include <gui/dyn/CTimer.h>
#include <gui/dyn/Context.h>

namespace gui::dyn {

std::optional<std::string> get_modifier_from_object(Drawable* object, const VarProps& value);

template<typename ApplyF, typename ErrorF, typename Result = typename cmn::detail::return_type<ApplyF>::type>
inline auto resolve_variable(const std::string_view& word, const Context& context, State& state, ApplyF&& apply, ErrorF&& error) -> Result {
    auto props = extractControls(word);
    
    try {
        if(context.has(props.name)) {
            CTimer ctimer("normal");
            if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
                return apply(*context.variable(props.name), props.parse(context, state));
            else
                static_assert(std::invocable<ApplyF, VarBase_t&, const VarProps&>);
            
        } else if(props.name == "if") {
            CTimer ctimer("if");
            VarProps p{
                .name = std::string(props.name),
                .optional = props.optional,
                .html = props.html,
            };
            
            p.parameters.reserve(props.parameters.size());
            for(auto &pr : props.parameters) {
                p.parameters.emplace_back((std::string)pr);
            }
            
            if(p.parameters.size() >= 2) {
                bool condition = convert_to_bool(parse_text(p.parameters.at(0), context, state));
                //print("Condition ", props.parameters.at(0)," => ", condition);
                if(condition)
                    return Meta::fromStr<Result>(parse_text(p.parameters.at(1), context, state));
                else if(p.parameters.size() == 3)
                    return Meta::fromStr<Result>(parse_text(p.parameters.at(2), context, state));
            }
            
        } else if(auto it = context.defaults.variables.find(props.name); it != context.defaults.variables.end()) {
            CTimer ctimer("custom var");
            return Meta::fromStr<Result>(it->second->value<std::string>(context, state));
        } else if(auto it = state._named_entities.find(props.name);
                  it != state._named_entities.end())
        {
            auto v = get_modifier_from_object(it->second.get(), props.parse(context, state));
            if(v.has_value()) {
                return Meta::fromStr<Result>(v.value());
            }
        }
        
    } catch(...) {
        // catch exceptions
    }
    
    if constexpr(std::invocable<ErrorF, bool>)
        return error(props.optional);
    else
        return error();
}

}
