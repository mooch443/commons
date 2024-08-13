#include "Context.h"
#include <gui/dyn/State.h>

namespace cmn::gui::dyn {

void CurrentObjectHandler::register_tooltipable(std::weak_ptr<LabeledField> field, std::weak_ptr<Drawable> ptr) {
    _textfields.emplace_back(field, ptr);
}

void CurrentObjectHandler::unregister_tooltipable(std::weak_ptr<LabeledField> field) {
    auto ptr = field.lock();
    if(not ptr)
        return; /// invalid input
    
    for(auto it = _textfields.begin(); it != _textfields.end();) {
        if(auto lock = std::get<0>(*it).lock();
           lock == nullptr)
        {
            it = _textfields.erase(it);
            
        } else if(lock == ptr) {
            _textfields.erase(it);
            return;
            
        } else {
            ++it;
        }
    }
}

void CurrentObjectHandler::update_tooltips(DrawStructure &graph) {
    std::shared_ptr<Drawable> found = nullptr;
    std::string name{"<null>"};
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto it = _textfields.begin(); it != _textfields.end(); ) {
        auto &[_field, _ptr] = *it;
        auto ptr = _field.lock();
        
        if(not ptr) {
            it = _textfields.erase(it);
            continue;
        } else {
            ++it;
        }
        
        ptr->text()->set_clickable(true);
        
        if(ptr->representative()->parent()
           && ptr->representative()->parent()->stage()
           && ptr->representative()->hovered() /*&& (ptr->representative().ptr.get() == graph.hovered_object() || dynamic_cast<Textfield*>(graph.hovered_object()))*/)
        {
            //found = ptr->representative();
            if(dynamic_cast<const LabeledCombobox*>(ptr.get())) {
                auto p = dynamic_cast<const LabeledCombobox*>(ptr.get());
                auto hname = p->highlighted_parameter();
                if(hname.has_value()) {
                    //Print("This is a labeled combobox: ", graph.hovered_object(), " with ", hname.value(), " highlighted.");
                    name = hname.value();
                    found = ptr->representative().get_smart();
                    break;
                } else {
                    //Print("This is a labeled combobox: ", graph.hovered_object(), " with nothing highlighted.");
                    hname = p->selected_parameter();
                    if(hname.has_value()) {
                        name = hname.value();
                        found = ptr->representative().get_smart();
                        break;
                    }
                }
            } else {
                found = ptr->representative().get_smart();
                auto r = ptr->ref();
                name = r.valid() ? r.name() : "<null>";
                break;
            }
            //if(ptr->representative().is<Combobox>())
            //ptr->representative().to<Combobox>()-
        }
    }
    
    if(found) {
        ref = std::make_unique<sprite::Reference>(dyn::settings_map()[name]);
    }

    if(found && ref && found->hovered())
    {
        //auto ptr = state._settings_tooltip.to<SettingsTooltip>();
        //if(ptr && ptr->other()
        //   && (not ptr->other()->parent() || not ptr->other()->parent()->stage()))
        {
            // dont draw
            //state._settings_tooltip.to<SettingsTooltip>()->set_other(nullptr);
        } //else {
        if(name.empty())
            Print("Error!");
        
        set_tooltip(name, found);
        add_tooltip(graph);
        //}
        
    } else {
        set_tooltip(nullptr);
    }
}

void CurrentObjectHandler::add_tooltip(DrawStructure &graph) {
    if(_tooltip_object)
        graph.wrap_object(*_tooltip_object);
}

void CurrentObjectHandler::set_tooltip(const std::string_view &parameter, std::weak_ptr<Drawable> other) {
    if(not _tooltip_object)
        _tooltip_object = std::make_shared<SettingsTooltip>();
    _tooltip_object->set_other(other);
    _tooltip_object->set_parameter((std::string)parameter);
}

void CurrentObjectHandler::set_tooltip(std::nullptr_t) {
    _tooltip_object = nullptr;
}

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
