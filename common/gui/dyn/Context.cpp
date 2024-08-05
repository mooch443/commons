#include "Context.h"
#include <gui/dyn/State.h>

namespace cmn::gui::dyn {

void CurrentObjectHandler::set_variable_value(std::string_view name, std::string_view value)
{
    _variable_values[std::string(name)] = std::string(value);
}

std::optional<std::string_view> CurrentObjectHandler::get_variable_value(std::string_view name) const
{
    if(auto it = _variable_values.find(name);
       it != _variable_values.end())
    {
        return std::string_view(it->second);
    }
    
    return std::nullopt;
}

std::shared_ptr<Drawable> CurrentObjectHandler::retrieve_named(std::string_view name)
{
    if(auto it = _named_entities.find(name);
       it != _named_entities.end())
    {
        auto lock = it->second.lock();
        if(not lock)
            _named_entities.erase(it);
        return lock;
    }
    return nullptr;
}

void CurrentObjectHandler::register_named(const std::string &name, std::weak_ptr<Drawable> ptr)
{
    _named_entities[name] = ptr;
}

void CurrentObjectHandler::reset() {
    _current_object.reset();
}

void CurrentObjectHandler::select(const std::shared_ptr<Drawable> & ptr) {
    _current_object = std::weak_ptr(ptr);
}

std::shared_ptr<Drawable> CurrentObjectHandler::get() const {
    return _current_object.lock();
}

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
