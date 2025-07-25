#include "DynamicGUI.h"
#include <gui/types/Button.h>
#include <gui/types/StaticText.h>
#include <misc/SpriteMap.h>
#include <file/DataLocation.h>
#include <gui/types/Textfield.h>
#include <gui/types/Checkbox.h>
#include <gui/types/Dropdown.h>
#include <misc/GlobalSettings.h>
#include <gui/types/SettingsTooltip.h>
#include <common/misc/default_settings.h>
#include <gui/ParseLayoutTypes.h>
#include <gui/types/ErrorElement.h>
#include <gui/DrawBase.h>
#include <gui/LabeledField.h>
#include <gui/dyn/ParseText.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>

namespace cmn::gui {
namespace dyn {

/*std::string Pattern::toStr() const {
    return "Pattern<" + std::string(original) + ">";
}*/

namespace Modules {
std::unordered_map<std::string, Module> mods;

void add(Module&& m) {
    auto name = m._name;
    mods.emplace(name, std::move(m));
}

void remove(const std::string& name) {
    if(mods.contains(name))
        mods.erase(name);
}

Module* exists(const std::string& name) {
    auto it = mods.find(name);
    if(it != mods.end()) {
        return &it->second;
    }
    return nullptr;
}

}

Layout::Ptr parse_object(GUITaskQueue_t* gui,
                         const glz::json_t::object_t& obj,
                         const Context& context,
                         State& state,
                         const DefaultSettings& defaults,
                         uint64_t hash)
{
    LayoutContext layout(gui, obj, state, context, defaults, hash);
    hash = layout.hash;

    try {
        Layout::Ptr ptr;

        switch (layout.type) {
            case LayoutType::each:
                ptr = layout.create_object<LayoutType::each>();
                break;
            case LayoutType::list:
                ptr = layout.create_object<LayoutType::list>();
                break;
            case LayoutType::condition:
                ptr = layout.create_object<LayoutType::condition>();
                break;
            case LayoutType::vlayout:
                ptr = layout.create_object<LayoutType::vlayout>();
                break;
            case LayoutType::hlayout:
                ptr = layout.create_object<LayoutType::hlayout>();
                break;
            case LayoutType::gridlayout:
                ptr = layout.create_object<LayoutType::gridlayout>();
                break;
            case LayoutType::collection:
                ptr = layout.create_object<LayoutType::collection>();
                break;
            case LayoutType::button:
                ptr = layout.create_object<LayoutType::button>();
                break;
            case LayoutType::circle:
                ptr = layout.create_object<LayoutType::circle>();
                break;
            case LayoutType::text:
                ptr = layout.create_object<LayoutType::text>();
                break;
            case LayoutType::stext:
                ptr = layout.create_object<LayoutType::stext>();
                break;
            case LayoutType::rect:
                ptr = layout.create_object<LayoutType::rect>();
                break;
            case LayoutType::textfield:
                ptr = layout.create_object<LayoutType::textfield>();
                break;
            case LayoutType::checkbox:
                ptr = layout.create_object<LayoutType::checkbox>();
                break;
            case LayoutType::settings:
                ptr = layout.create_object<LayoutType::settings>();
                break;
            case LayoutType::combobox:
                ptr = layout.create_object<LayoutType::combobox>();
                break;
            case LayoutType::line:
                ptr = layout.create_object<LayoutType::line>();
                break;
            case LayoutType::image:
                ptr = layout.create_object<LayoutType::image>();
                break;
            default:
                if (auto it = context.custom_elements.find(obj.at("type").get_string());
                    it != context.custom_elements.end())
                {
                    if(auto sit = state._collectors->objects.find(hash);
                       sit != state._collectors->objects.end()
                       && std::holds_alternative<CustomBody>(sit->second->object))
                    {
                        /// we are a custom object already!
                        auto &obj = *sit->second;
                        auto &body = std::get<CustomBody>(obj.object);
                        ptr = body._customs_cache;
                        context.custom_elements.at(body.name)->update(ptr, context, state, obj.patterns);
                        obj.timer.reset();
                        
                    } else {
                        ptr = it->second->create(layout);
                        auto obj = state.register_variant(hash, ptr, CustomBody{
                            .name = it->first,
                            ._customs_cache = ptr
                        });
                        auto &body = std::get<CustomBody>(obj->object);
                        body._customs_cache = ptr;
                        it->second->update(ptr, context, state, obj->patterns);
                        obj->timer.reset();
                    }
                } else
                    throw InvalidArgumentException("Unknown layout type: ", layout.type);
                break;
        }
        
        if(not ptr) {
            FormatExcept("Cannot create object ",obj.at("type").get_string());
            return nullptr;
        }
        
        layout.finalize(ptr);
        return ptr;
    } catch(const std::exception& e) {
        std::string text = "<b><red>Failed to make object with '"+std::string(e.what())+"' for</red></b>: <c>"+ glz::write_json(obj).value_or("null")+"</c>";
        FormatExcept("Failed to make object here ",e.what(), " for ",glz::write_json(obj).value_or("null"));
        return Layout::Make<ErrorElement>(attr::Str{text}, Loc{layout.pos}, Size{layout.size});
    }
}

tl::expected<std::tuple<DefaultSettings, glz::json_t>, std::string> load(const std::string& text){
    DefaultSettings defaults;
    try {
        glz::json_t obj{};
        auto error = glz::read_json(obj, text);
        if(error != glz::error_code::none)
            return tl::unexpected(glz::format_error(error, text));
        
        State state;
        try {
            if(obj.contains("defaults") && obj.at("defaults").is_object()) {
                auto& d = obj.at("defaults").get_object();
                defaults.font = parse_font(d, defaults.font);
                if(d.contains("color")) defaults.textClr = parse_color(d["color"]);
                if(d.contains("fill")) defaults.fill = parse_color(d["fill"]);
                if(d.contains("line")) defaults.line = parse_color(d["line"]);
                if(d.contains("highlight_clr")) defaults.highlightClr = parse_color(d["highlight_clr"]);
                if(d.contains("window_color")) defaults.window_color = parse_color(d["window_color"]);
                if(d.contains("pad"))
                    defaults.pad = Meta::fromStr<Bounds>(glz::write_json(d["pad"]).value());
                
                if(d.contains("vars") && d["vars"].is_object()) {
                    for(auto &[name, value] : d["vars"].get_object()) {
                        defaults.variables[name] = std::unique_ptr<VarBase<const Context&, State&>>(new Variable([value = Meta::fromStr<std::string>(glz::write_json(value).value())](const Context& context, State& state) -> std::string {
                            return parse_text(value, context, state);
                        }));
                    }
                }
            }
        } catch(const std::exception& ex) {
            //FormatExcept("Cannot parse layout due to: ", ex.what());
            //return tl::unexpected(ex.what());
            throw InvalidSyntaxException(ex.what());
        }
        return std::make_tuple(defaults, obj["objects"]);
        
    } catch(const std::exception& error) {
        throw InvalidSyntaxException(error.what());
        //return tl::unexpected(error.what());
    }
}

bool DynamicGUI::update_objects(GUITaskQueue_t* gui, DrawStructure& g, Layout::Ptr& o, const Context& context, State& state) {
    auto hash = (std::size_t)o->custom_data("object_index");
    if(not hash) {
        //! if this is a Layout type, need to iterate all children as well:
        if(o.is<Layout>()) {
            auto layout = o.to<Layout>();
            auto& objects = layout->objects();
            bool changed = false;
            for(size_t i=0, N = objects.size(); i<N; ++i) {
                auto& child = objects[i];
                auto r = update_objects(gui, g, child, context, state);
                if(r) {
                    // objects changed
                    changed = true;
                    changed = layout->replace_child(i, child) || changed;
                }
            }
            if(changed) {
                //Print("Changed layout: ", *layout);
                layout->set_layout_dirty();
            }
        } else {
            //Print("Object ", *o, " has no hash and is thus not updated.");
        }
        return false;
    }
    
    assert(not o || o.get_smart());
    if(auto lock = state._current_object_handler.lock();
       lock && o)
    {
        lock->select(o.get_smart());
    } else if(lock) {
        lock->reset();
    } else {
#ifndef NDEBUG
        throw std::runtime_error("Have no current_object_handler!");
#endif
    }
    
    /// try to find the current object
    auto it = state._collectors->objects.find(hash);
    if(it == state._collectors->objects.end())
        return false;
    
    if(not it->second) {
        state._collectors->objects.erase(it);
        return false;
    }
    
    /// get the hashed object and ensure
    /// it stays alive during execution
    auto ptr = it->second;
    return ptr->update(gui, hash, g, o, context, state);
}

void DynamicGUI::reload(DrawStructure& graph) {
    if(not first_load
       && last_load.elapsed() < 0.25)
    {
        return;
    }
    
    if(not read_file_future.valid()) {
        read_file_future = std::async(std::launch::async,
             [this]() -> tl::expected<std::tuple<DefaultSettings, glz::json_t>, std::string>
             {
                try {
                    auto p = file::DataLocation::parse("app", path).absolute();
                    auto text = p.read_file();
                    if(previous != text) {
                        previous = text;
                        return load(text);
                    }
                    
                } catch(const std::invalid_argument&) {
                } catch(const std::exception& e) {
                    FormatExcept("Error loading gui layout from file: ", e.what());
                } catch(...) {
                    FormatExcept("Error loading gui layout from file");
                }
                
                return tl::unexpected("No news.");
            });
        
    }
    
    if(read_file_future.valid()
       && (read_file_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready
           || first_load))
    {
        read_file_future.get().transform([&, graph = &graph](const auto& result) {
            auto&& [defaults, layout] = result;
            //state._text_fields.clear();
            context.defaults = std::move(defaults);
            
            State tmp;
            tmp._current_object_handler = std::weak_ptr(current_object_handler);
            
            auto last_selected = graph->selected_object();
            if(state._last_settings_box) {
                tmp._last_settings_box = std::move(state._last_settings_box);
                tmp._settings_was_selected = graph->selected_object() ? (graph->selected_object()->is_child_of(tmp._last_settings_box.get()) || graph->selected_object() == tmp._last_settings_box.get()) : false;
            }
            
            context.system_variables().emplace(VarFunc("hovered", [object_handler = tmp._current_object_handler](const VarProps& props) -> bool {
                if(props.parameters.empty()) {
                    auto lock = object_handler.lock();
                    if(lock) {
                        if(auto ptr = lock->get();
                           ptr != nullptr)
                        {
                            return ptr->hovered();
                        }
                    }
                    
                } else if(props.parameters.size() == 1) {
                    if(auto lock = object_handler.lock();
                       lock != nullptr)
                    {
                        if(auto ptr = lock->retrieve_named(props.first());
                           ptr != nullptr)
                        {
                            return ptr->hovered();
                        }
                    }
                }
                return false;
            }));
            
            context.system_variables().emplace(VarFunc("pos", [object_handler = tmp._current_object_handler](const VarProps& props) -> Size2 {
                if(props.parameters.empty()) {
                    auto lock = object_handler.lock();
                    if(lock) {
                        if(auto ptr = lock->get();
                           ptr != nullptr)
                        {
                            return ptr->global_bounds().pos();
                        }
                    }
                    
                } else if(props.parameters.size() == 1) {
                    if(auto lock = object_handler.lock();
                       lock != nullptr)
                    {
                        if(auto ptr = lock->retrieve_named(props.first());
                           ptr != nullptr)
                        {
                            return ptr->global_bounds().pos();
                        }
                    }
                }
                return false;
            }));
            
            context.system_variables().emplace(VarFunc("dimensions", [object_handler = tmp._current_object_handler](const VarProps& props) -> Size2 {
                if(props.parameters.empty()) {
                    auto lock = object_handler.lock();
                    if(lock) {
                        if(auto ptr = lock->get();
                           ptr != nullptr)
                        {
                            return ptr->size();
                        }
                    }
                    
                } else if(props.parameters.size() == 1) {
                    if(auto lock = object_handler.lock();
                       lock != nullptr)
                    {
                        if(auto ptr = lock->retrieve_named(props.first());
                           ptr != nullptr)
                        {
                            return ptr->size();
                        }
                    }
                }
                return false;
            }));
            
            context.system_variables().emplace(VarFunc("selected", [object_handler = tmp._current_object_handler](const VarProps& props) -> bool {
                if(props.parameters.empty()) {
                    auto lock = object_handler.lock();
                    if(lock) {
                        if(auto ptr = lock->get();
                           ptr != nullptr)
                        {
                            return ptr->selected();
                        }
                    }
                    
                } else if(props.parameters.size() == 1) {
                    if(auto lock = object_handler.lock();
                       lock != nullptr)
                    {
                        if(auto ptr = lock->retrieve_named(props.first());
                           ptr != nullptr)
                        {
                            return ptr->selected();
                        }
                    }
                }
                return false;
            }));
            
            Print(" * initialized system variables with ", extract_keys(context.system_variables()));
            
            std::vector<Layout::Ptr> objs;
            objects.clear();
            state = {};
            for(auto &obj : layout.get_array()) {
                auto ptr = parse_object(gui, obj.get_object(), context, tmp, context.defaults);
                if(ptr) {
                    objs.push_back(ptr);
                }
            }
            
            if(tmp._settings_was_selected)
            {
                if(current_object_handler->_tooltip_object
                   && current_object_handler->_tooltip_object->is_staged())
                {
                    tmp._mark_for_selection = last_selected;
                }
            }
            
            state = std::move(tmp);
            objects = std::move(objs);
        });
        
        last_load.reset();
    }
    
    if(first_load)
        first_load = false;
}

void DynamicGUI::update(DrawStructure& graph, Layout* parent, const std::function<void(std::vector<Layout::Ptr>&)>& before_add)
{
    reload(graph);
    
    context._last_mouse = graph.mouse_position();
    
    /*if(state.ifs.size() > 3000) {
        for(auto it = state.ifs.begin(); it != state.ifs.end() && state.ifs.size() > 2000;) {
            if(it->second._if
               && not it->second._if->is_displayed()
               && (not it->second._else || not it->second._else->is_displayed()))
            {
                it = state.ifs.erase(it);
            } else
                ++it;
        }
        
        while(state.ifs.size() > 2000) {
            state.ifs.erase(state.ifs.begin());
        }
    }
    
    if(state._customs_cache.size() > 5000) {
        std::vector<std::tuple<double, size_t>> sorted_objects;
        sorted_objects.resize(state._timers.size());
        for(auto &[h, o] : state._timers) {
            sorted_objects.push_back({o.elapsed(), h});
        }
        std::sort(sorted_objects.begin(), sorted_objects.end());
        
        for(auto it = --sorted_objects.end(); it != sorted_objects.begin(); --it ) {
            if(state._customs_cache.size() <= 3000)
                break;
            
            auto& [e, h] = *it;
            
            Print("Deleting object ",h ," that has not been visible for ", e, " seconds.");

            state._timers.erase(h);
            state._customs.erase(h);
            state._customs_cache.erase(h);
        }
    }*/
    
    bool do_update_objects{false};
    if(not first_update
       && last_update.elapsed() < 0.01)
    {
        //
    } else {
        do_update_objects = true;
    }
    
    if(first_update)
        first_update = false;
    
    /*if(do_update_objects && state._collectors->_timers.size() > 100000) {
        Print("* Clearing object cache by reloading... ", state._collectors->_timers.size());
        first_load = true;
        if(read_file_future.valid())
            read_file_future.wait();
        previous.clear();
        reload();
    }*/
    
    if(context.defaults.window_color != Transparent) {
        if(base)
            base->set_background_color(context.defaults.window_color);
        else {
            static std::once_flag flag;
            std::call_once(flag, [&](){
                FormatWarning("Cannot set background color to ", context.defaults.window_color);
            });
        }
    }
    
    
    //! clear variable state
    current_object_handler->_variable_values.clear();
    
#ifndef NDEBUG
    if(_debug_timer.elapsed() > 10) {
        size_t overall_count{0};
        
        std::queue<HashedObject*> queue;
        overall_count += state._collectors->objects.size();
        
        for(auto &[_, ptr] : state._collectors->objects)
            queue.push(ptr.get());
        
        while(not queue.empty()) {
            auto front = queue.front();
            queue.pop();
            
            front->run_if_one_of<IfBody, LoopBody, ListContents>([&](auto&& object) {
                if(not object._state) {
                    return;
                }
                
                overall_count += object._state->_collectors->objects.size();
                
                //object._state->_variable_values.clear();
                for(auto &[_, ptr] : object._state->_collectors->objects)
                    queue.push(ptr.get());
            });
        }
        
        Print("[dyn::update] #objects estimate: ", overall_count);
        _debug_timer.reset();
    }
#endif
    
    static Timing timing("dyn::update", 10);
    if(TakeTiming take(timing);
       parent)
    {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            parent->set_children(copy);
        } else {
            parent->set_children(objects);
        }
        
        for(auto &obj : objects) {
            if(do_update_objects)
                update_objects(gui, graph, obj, context, state);
        }
        
    } else {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            for(auto &obj : copy) {
                if(do_update_objects)
                    update_objects(gui, graph, obj, context, state);
                graph.wrap_object(*obj);
            }
            
        } else {
            for(auto &obj : objects) {
                if(do_update_objects)
                    update_objects(gui, graph, obj, context, state);
                graph.wrap_object(*obj);
            }
        }
    }
    
    dyn::update_tooltips(graph, state);
    
    if(state._mark_for_selection) {
        if(state._mark_for_selection->is_child_of(&graph.root())) {
#ifndef NDEBUG
            Print("Had to reselect object ", state._mark_for_selection, " after reloading the GUI.");
#endif
            graph.select(state._mark_for_selection);
            state._mark_for_selection = nullptr;
        } else {
#ifndef NDEBUG
            FormatWarning("Marked for selection, but unvailable: ", state._mark_for_selection);
#endif
        }
    }

    //if(state._collectors->_timers.size() > 10000) {
        /*std::vector<std::tuple<double, size_t>> sorted_objects;
        sorted_objects.resize(state._timers.size());
        for(auto &[h, o] : state._timers) {
            sorted_objects.push_back({o.elapsed(), h});
        }
        std::sort(sorted_objects.begin(), sorted_objects.end());
        
        for(auto it = --sorted_objects.end(); it != sorted_objects.begin(); --it) {
            if(state._timers.size() <= 5000)
                break;
            
            auto [e, h] = *it;
            
            Print("Deleting object ",h ," that has not been visible for ", e, " seconds.");
            
            if(auto kit = state._customs.find(h);
               kit != state._customs.end())
            {
                state._customs.erase(kit);
                assert(state._customs_cache.contains(h));
                state._customs_cache.erase(h);
                
            } else if(auto fit = state.ifs.find(h);
                      fit != state.ifs.end())
            {
                state.ifs.erase(fit);
                
            } else {
                FormatWarning("Unknown type of object in timers object ", h);
            }
            
            state._timers.erase(h);
        }*/
        
    //}
    
    if (do_update_objects)
        last_update.reset();
}

DynamicGUI::operator bool() const {
    return not path.empty();
}

void DynamicGUI::clear() {
    apply_to_objects(objects, [](Drawable* c) {
        if(c->type() == Type::ENTANGLED
           || c->type() == Type::SECTION)
        {
            auto ptr = static_cast<SectionInterface*>(c);
            ptr->set_stage(nullptr);
        }
    });
    path = {};
    objects.clear();
    previous.clear();
    state = {};
    first_load = true;
    context = {};
}

void update_tooltips(DrawStructure& graph, State& state) {
    auto handler = state._current_object_handler.lock();
    if(not handler)
        return;
    
    handler->update_tooltips(graph);
}

}
}
