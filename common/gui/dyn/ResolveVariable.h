#pragma once
#include <commons.pc.h>
#include <gui/dyn/ParseText.h>
#include <gui/dyn/VarProps.h>
#include <gui/dyn/State.h>
#include <gui/dyn/CTimer.h>
#include <gui/dyn/Context.h>

namespace cmn::gui::dyn {

std::optional<std::string> get_modifier_from_object(Drawable* object, const VarProps& value);

template<typename T, typename Str>
T resolve_variable_type(Str _word, const Context& context, State& state);

template<typename ApplyF, typename ErrorF, typename Result = typename cmn::detail::return_type<ApplyF>::type>
inline auto resolve_variable(const std::string_view& word, const Context& context, State& state, ApplyF&& apply, ErrorF&& error) -> Result {
    auto props = extractControls(word);
    
    try {
        if(context.has(props.name)) {
            [[maybe_unused]] CTimer ctimer("normal");
            if constexpr(std::invocable<ApplyF, VarBase_t&, const VarProps&>)
                return apply(*context.variable(props.name), props.parse(context, state));
            else
                static_assert(std::invocable<ApplyF, VarBase_t&, const VarProps&>);
            
        } else if(props.name == "if") {
            [[maybe_unused]] CTimer ctimer("if");
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
                //bool condition = resolve_variable_type<bool>(p.parameters.at(0), context, state);
                //Print("Condition ", props.parameters.at(0)," => ", condition);
                if(condition)
                    return Meta::fromStr<Result>(parse_text(p.parameters.at(1), context, state));
                else if(p.parameters.size() == 3)
                    return Meta::fromStr<Result>(parse_text(p.parameters.at(2), context, state));
            }
            
        } else if(auto it = context.defaults.variables.find(props.name); it != context.defaults.variables.end()) {
            [[maybe_unused]] CTimer ctimer("custom var");
            return Meta::fromStr<Result>(it->second->value<std::string>(context, state));
            
        } else if(auto lock = state._current_object_handler.lock();
                  lock != nullptr)
        {
            if(auto ptr = lock->retrieve_named(props.name);
               ptr != nullptr)
            {
                auto v = get_modifier_from_object(ptr.get(), props.parse(context, state));
                if(v.has_value()) {
                    return Meta::fromStr<Result>(v.value());
                }
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

template<typename MapType, typename K>
    requires (std::same_as<MapType, sprite::Map> || std::same_as<MapType, sprite::Map&>)
bool resolve_map_to_bool(const VarBase_t& variable, const VarProps& modifiers) {
    MapType tmp = variable.value<MapType>({});
    if(modifiers.subs.empty()) {
        return not tmp.empty();
    }
    
    auto ref = tmp[modifiers.subs.front()];
    if(not ref.get().valid()) {
        FormatWarning("Retrieving invalid property (", modifiers, ") from map with keys ", tmp.keys(), ".");
        return false;
    }
    
    //Print(ref.get().name()," = ", ref.get().valueString(), " with ", ref.get().type_name());
    if constexpr(std::same_as<K, bool>) {
        if(ref.template is_type<bool>())
            return ref.template value<bool>();
        else
            return convert_to_bool(ref.get().valueString());
        
    } else if(ref.template is_type<K>()) {
        return ref.get().template value<K>();
    } else if(ref.template is_type<std::string>()) {
        return not ref.get().template value<std::string>().empty();
    } else if(ref.template is_type<file::Path>()) {
        return not ref.get().template value<file::Path>().empty();
    }
    
    throw InvalidArgumentException("Cannot convert ",modifiers," to bool for variable ", variable.class_name());
}

template<typename T, typename Str>
T resolve_variable_type(Str _word, const Context& context, State& state) {
    std::string_view word;
    if constexpr(std::same_as<Str, Pattern>) {
        word = std::string_view(_word.original);
    } else
        word = std::string_view(_word);
    
    if(word.empty())
        throw U_EXCEPTION("Invalid variable name (is empty).");
    
    if(word.length() > 2
       && ((word.front() == '{'
            && word.back() == '}')
       || (word.front() == '['
            && word.back() == ']'))
       )
    {
        word = word.substr(1,word.length()-2);
    }
    
    return resolve_variable(word, context, state, [&](const VarBase_t& variable, const VarProps& modifiers) -> T
    {
        if constexpr(std::is_same_v<T, bool>) {
            if(variable.is<bool>()) {
                return variable.value<T>(modifiers);
            } else if(variable.is<file::Path>()) {
                auto tmp = variable.value<file::Path>(modifiers);
                return not tmp.empty();
            } else if(variable.is<std::string>()) {
                auto tmp = variable.value<std::string>(modifiers);
                return not tmp.empty();
            } else if(variable.is<sprite::Map>()) {
                return resolve_map_to_bool<sprite::Map, T>(variable, modifiers);
                
            } else if(variable.is<sprite::Map&>()) {
                return resolve_map_to_bool<sprite::Map&, T>(variable, modifiers);
            }
        }
        
        if(variable.is<T>()) {
            return variable.value<T>(modifiers);
            
        } else if(variable.is<sprite::Map&>()) {
            auto& tmp = variable.value<sprite::Map&>({});
            if(not modifiers.subs.empty()) {
                auto ref = tmp[modifiers.subs.front()];
                if(ref.template is_type<T>()) {
                    return ref.get().template value<T>();
                }
                
            } else
                throw U_EXCEPTION("sprite::Map selector is empty, need subvariable doing ", word,".sub");
            
        } else if(variable.is<VarReturn_t>()) {
            auto tmp = variable.value<VarReturn_t>(modifiers);
            if(tmp.template is_type<T>()) {
                return tmp.get().template value<T>();
            }
        }
        
        throw U_EXCEPTION("Invalid variable type for ",word," or variable not found.");
        
    }, [&]() -> T {
        throw U_EXCEPTION("Invalid variable type for ",word," or variable not found.");
    });
}


}
