#include "State.h"
#include <gui/LabeledField.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>
#include <gui/DynamicGUI.h>
#include <gui/Passthrough.h>
#include <misc/default_settings.h>

namespace cmn::gui::dyn {


bool HashedObject::update(GUITaskQueue_t *gui, size_t hash, DrawStructure& g, Layout::Ptr &o, const Context &context, State &state)
{
    //! something that needs to be executed before everything runs
    if(display_fn.has_value()) {
        display_fn.value()(g);
    }

    if (std::holds_alternative<CustomBody>(object)) {
        if(not context.custom_elements.at(std::get<CustomBody>(object).name)->update(o, context, state, patterns))
        {
            if(update_patterns(gui, hash, o, context, state)) {
                //changed = true;
            }
        }
        return false;
    }
    
    //! if statements below
    if(std::holds_alternative<IfBody>(object)) {
        return update_if(gui, hash, g, o, context, state);
    }
    
    //! for-each loops below
    if(std::holds_alternative<LoopBody>(object)) {
        return update_loops(gui, hash, g, o, context, state);
    }
    
    //! did the current object change?
    bool changed{false};
    
    //! fill default fields like fill, line, pos, etc.
    if(update_patterns(gui, hash, o, context, state)) {
        changed = true;
    }
    
    //! if this is a Layout type, need to iterate all children as well:
    if(o.is<Layout>()) {
        auto layout = o.to<Layout>();
        auto& objects = layout->objects();
        for(size_t i=0, N = objects.size(); i<N; ++i) {
            auto& child = objects[i];
            auto r = DynamicGUI::update_objects(gui, g, child, context, state);
            if(r) {
                // objects changed
                layout->replace_child(i, child);
            }
        }
    }
    
    //! list contents loops below
    if(std::holds_alternative<ListContents>(object)) {
        (void)update_lists(gui, hash, g, o, context, state);
    } else if(std::holds_alternative<ManualListContents>(object)) {
        (void)update_manual_lists(gui, hash, g, o, context, state);
    }
    return changed;
}

bool HashedObject::update_if(GUITaskQueue_t *gui, uint64_t, DrawStructure& g, Layout::Ptr &o, const Context &context, State &state)
{
    auto& obj = std::get<IfBody>(object);
    
    if(not o.is<Fallthrough>()) {
        state._collectors->dealloc(obj._assigned_hash);
        //it = state._collectors->ifs.erase(it);
        return true;
    }
    try {
        //auto res = resolve_variable_type<bool>(obj.variable, context, state);
        const bool res = convert_to_bool(obj.variable.realize(context, state));
        auto last_condition = (uint64_t)o->custom_data("last_condition");
        auto pass = o.to<Fallthrough>();
        
        //IndexScopeHandler handler{state._current_index};
        if(not res) {
            if(obj._if) {
                obj._if = nullptr;
                pass->set_object(nullptr);
            }
            
            if(not obj.__else.is_null()) {
                if(not obj._else) {
                    for(auto &[n, p] : patterns) {
                        //obj._state->_collectors->objects[hash][n] = Pattern{p};
                        obj._state->register_pattern(1, n, Pattern{p});
                    }
                    
                    obj._state->_collectors->objects.clear();
                    obj._state->_current_index = {};
                    //obj._state->_variable_values.clear();
                    
                    obj._else = parse_object(gui, obj.__else.get_object(), context, *obj._state, context.defaults);
                } //else
                    //state._current_index.inc();
                
                if(last_condition != 1) {
                    auto ref = obj._else;
                    
                    ref->set_bounds_changed();
                    pass->set_object(obj._else);
                    
                    if(ref != obj._else) {
                        FormatWarning("Differs! ", *ref, " vs. ", *obj._else);
                        return true;
                    }
                }
                
                if(DynamicGUI::update_objects(gui, g, obj._else, context, *obj._state)) {
                    //FormatWarning("Object changed after update");
                }
                
            } else {
                if(o->is_displayed()) {
                    o->set_is_displayed(false);
                    pass->set_object(nullptr);
                }
                
                obj._else = nullptr;
            }
            
            if(last_condition != 1) {
                o->add_custom_data("last_condition", (void*)1);
            }
            
            timer.reset();
            return last_condition != 1;
            
        } else {
            if(obj._else) {
                obj._else = nullptr;
                pass->set_object(nullptr);
            }
            
            if(not obj._if) {
                for(auto &[n, p] : patterns) {
                    //obj._state->_collectors->objects[hash][n] = Pattern{p};
                    obj._state->register_pattern(1, n, Pattern{p});
                }
                
                obj._state->_collectors->objects.clear();
                obj._state->_current_index = {};
                
                obj._if = parse_object(gui, obj.__if.get_object(), context, *obj._state, context.defaults);
            } //else
                //state._current_index.inc();
            
            if(last_condition != 2) {
                o->set_is_displayed(true);
                o->add_custom_data("last_condition", (void*)2);
                pass->set_object(obj._if);
            }
            
            auto ref = obj._if;
            if(DynamicGUI::update_objects(gui, g, obj._if, context, *obj._state)) {
                //FormatWarning("Object changed after update.");
            }
            if(ref != obj._if)
                FormatWarning("Differs! ", *ref, " vs. ", *obj._if);
            
            timer.reset();
            return last_condition != 2;
        }
        
    } catch(const std::exception& ex) {
        FormatError(ex.what());
    }
    
    return false;
}

#if false
struct TimingInfo {
    std::string name;
    double time{0};
    uint64_t samples{0};
    
    void add(double seconds) {
        samples++;
        time += seconds;
        
        if(int64_t(time / double(samples)) % 3 == 0) {
            time = time / double(samples);
            samples = 1;
            Print(name,": ", time * 1000.0 * 1000.0, "us");
        }
    }
};
#endif

bool HashedObject::update_patterns(GUITaskQueue_t* gui, uint64_t hash, Layout::Ptr &o, const Context &context, State &state) {
    bool changed{false};
    
    LabeledField *field{_text_field.get()};
    const auto patterns_end = patterns.end();
    //Print("Found pattern ", pattern, " at index ", hash);
    
    if(patterns.contains("var")) {
        try {
            auto var = patterns.at("var").realize(context, state);
            //auto var = parse_text(patterns.at("var"), context, state);
            if(_text_field) {
                auto str = _text_field->ref().get().valueString();
                if(not _var_cache.has_value()
                   || _var_cache->_var != var)
                {
                    if(_var_cache.has_value()) {
                        Print("Need to change ", _var_cache->_value," => ", str, " and ", _var_cache->_var, " => ", var);
                        
                        //! replace old object (also updates the cache)
                        o = parse_object(gui, _var_cache->_obj, context, state, context.defaults, hash);
                        
                        field = _text_field.get();
                        changed = true;
                    } else {
                        throw std::runtime_error("VarCache was not initialized.");
                    }
                    
                } //else
                    //state._current_index.inc();
            }
        } catch(std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    auto ptr = o;
    if(field) {
        ptr = field->representative();
    }
    
    auto check_field = [&]<typename SourceType, typename TargetType>(std::string_view name) {
        
        auto it = patterns.find(name);
        if(it == patterns_end)
            return;
        try {
#if false
            static std::unordered_map<std::string, TimingInfo, MultiStringHash, MultiStringEqual> timings;
            Timer timer;
#endif
            auto text = it->second.realize(context, state);
            auto fill = Meta::fromStr<SourceType>(text);
            LabeledField::delegate_to_proper_type(TargetType{fill}, ptr);
#if false
            auto seconds = timer.elapsed();
            auto it = timings.find(name);
            if(it == timings.end()) {
                timings[(std::string)name].name = (std::string)name;
                it = timings.find(name);
            }
            it->second.add(seconds);
#endif
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    };
    
    check_field.operator()<Color, FillClr>("fill");
    check_field.operator()<Color, TextClr>("color");
    check_field.operator()<Color, LineClr>("line");
    check_field.operator()<Color, CellFillClr>("cellfillclr");
    check_field.operator()<Color, CellLineClr>("celllineclr");
    check_field.operator()<uint16_t, CellFillInterval>("cellfillinterval");
    check_field.operator()<Size2, MinCellSize>("mincellsize");
    check_field.operator()<std::string, Placeholder_t>("placeholder");
    check_field.operator()<CornerFlags_t, CornerFlags_t>("corners");
    check_field.operator()<Bounds, Margins>("pad");
    check_field.operator()<Float2_t, Alpha>("alpha");
    check_field.operator()<Vec2, Loc>("pos");
    check_field.operator()<Vec2, Scale>("scale");
    check_field.operator()<Vec2, Origin>("origin");
    
    if(auto it = patterns.find("size");
       it != patterns_end)
    {
        try {
            //auto& size = patterns.at("size");
            /*if(size.original == "auto")
            {
                if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
            } else {*/
            auto text = it->second.realize(context, state);
            if(text == "auto") {
                if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
            } else {
                ptr->set_size(Meta::fromStr<Size2>(text));
            }
                //o->set_size(resolve_variable_type<Size2>(size, context));
            //}
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    check_field.operator()<Size2, SizeLimit>("max_size");
    check_field.operator()<Size2, SizeLimit>("preview_max_size");
    check_field.operator()<Color, ItemBorderColor_t>("item_line");
    check_field.operator()<Color, ItemColor_t>("item_color");
    check_field.operator()<Color, LabelBorderColor_t>("label_line");
    check_field.operator()<Color, LabelColor_t>("label_fill");
    check_field.operator()<Size2, LabelDims_t>("label_size");
    check_field.operator()<Size2, ListDims_t>("list_size");
    check_field.operator()<Color, ListLineClr_t>("list_line");
    check_field.operator()<Color, ListFillClr_t>("list_fill");
    check_field.operator()<std::string, Str>("text");
    check_field.operator()<float, Radius>("radius");
    check_field.operator()<std::vector<Vec2>, Line::Points_t>("points");

    /*if(ptr.is<ExternalImage>()) {
        if(patterns.contains("path")) {
            auto img = ptr.to<ExternalImage>();
            auto output = file::Path(parse_text(patterns.at("path"), context, state));
            if(output.exists() && not output.is_folder()) {
                auto modified = output.last_modified();
                auto &entry = state._image_cache[output.str()];
                Image::Ptr ptr;
                if(std::get<0>(entry) != modified)
                {
                    ptr = load_image(output);
                    entry = { modified, Image::Make(*ptr) };
                } else if(state.chosen_images[hash] != output.str()) {
                    //ptr = Image::Make(*std::get<1>(entry));
                    //if(not img->source()
                    //   || *std::get<1>(entry) != *img->source())
                    {
                        img->unsafe_get_source().create(*std::get<1>(entry));
                        img->updated_source();
                    }
                }
                
                if(ptr) {
                    state.chosen_images[hash] = output.str();
                    img->set_source(std::move(ptr));
                }
            }
        }
    }*/

    return changed;
}

bool HashedObject::update_lists(GUITaskQueue_t*, uint64_t, DrawStructure &, const Layout::Ptr &o, const Context &context, State &state)
{
    ListContents &obj = std::get<ListContents>(object);
    
    size_t index=0;
    auto convert_to_item = [&gc = context, &index, &obj](ListContents::ItemTemplate& item_template, Context& context, State& state) -> DetailTooltipItem
    {
        DetailTooltipItem item;
        if(item_template.text) {
            item.set_name(item_template.text.value().pat.realize(context, state));
        }
        if(item_template.detail) {
            item.set_detail(item_template.detail.value().pat.realize(context, state));
        }
        if(item_template.tooltip) {
            item.set_tooltip(item_template.tooltip.value().pat.realize(context, state));
        }
        if(item_template.disabled) {
            item.set_disabled(item_template.disabled.value().pat.realize(context, state) == "true");
        } else
            item.set_disabled(false);
        
        if(item_template.action) {
            auto action = PreAction::fromStr(item_template.action.value().pat.realize(context, state));
            
            if(not obj.on_select_actions.contains(index)
               || std::get<0>(obj.on_select_actions.at(index)) != action.name)
            {
                obj.on_select_actions[index] = std::make_tuple(
                    index, [&gc = gc, index = index, action = action, context](){
                        Print("Clicked item at ", index, " with action ", action);
                        State state;
                        Action _action = action.parse(context, state);
                        if(_action.parameters.empty())
                            _action.parameters = { Meta::toStr(index) };
                        if(auto it = gc.actions.find(action.name); it != gc.actions.end()) {
                            try {
                                it->second(_action);
                            } catch(const std::exception& ex) {
                                // pass
                            }
                        }
                    }
                );
            }
        }
        
        return item;
    };
    
    if(context.has(obj.variable)) {
        if(context.variable(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
            
            auto& vector = context.variable(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>({});
            
            IndexScopeHandler handler{state._current_index};
            //if(vector != obj.cache) {
            std::vector<DetailTooltipItem> ptrs;
            //obj.cache = vector;
            obj._state = std::make_unique<State>();
            obj._state->_current_object_handler = state._current_object_handler;
            
            Context tmp = context;
            
            
            for(auto &v : vector) {
                //auto previous = state._variable_values;
                tmp.variables["i"] = v;
                try {
                    //auto &ref = v->value<sprite::Map&>({});
                    auto item = convert_to_item(obj.item, tmp, state);
                    ptrs.emplace_back(std::move(item));
                    ++index;
                } catch(const std::exception& ex) {
                    FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item).value_or("null"), " and type ", v->class_name());
                }
                //state._variable_values = std::move(previous);
            }
            
            o.to<ScrollableList<DetailTooltipItem>>()->set_items(ptrs);
        }
        else {
            auto variable = context.variable(obj.variable);
            Print("Variable ", obj.variable, " has unknown type ", variable->class_name(),".");
            
        }
    }
    else if(auto text = parse_text(obj.variable, context, state);
            not text.empty()
            && utils::beginsWith(text, '[')
            && utils::endsWith(text, ']'))
    {
        auto vector = Meta::fromStr<std::vector<std::string>>(text);
        
        IndexScopeHandler handler{state._current_index};
        std::vector<DetailTooltipItem> ptrs;
        obj._state = std::make_unique<State>();
        obj._state->_current_object_handler = state._current_object_handler;
        
        Context tmp = context;
        for(auto &v : vector) {
            //auto previous = state._variable_values;
            tmp.variables["i"] = VarFunc("i", [v](const VarProps&) -> std::string{
                return v;
            }).second;
            
            try {
                auto item = convert_to_item( obj.item, tmp, state);
                ptrs.emplace_back(std::move(item));
                ++index;
            } catch(const std::exception& ex) {
                FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item).value_or("null"), " and type string.");
            }
            //state._variable_values = std::move(previous);
        }
        
        o.to<ScrollableList<DetailTooltipItem>>()->set_items(ptrs);
    }
    return false;
}

bool HashedObject::update_manual_lists(GUITaskQueue_t *, uint64_t, DrawStructure &, const Layout::Ptr &o, const Context &context, State &state) {
    ManualListContents &body = std::get<ManualListContents>(object);
    std::vector<DetailTooltipItem> output{body.rendered};
    
    for (size_t i = 0, N = body.items.size(); i < N; ++i) {
        auto& item = body.items[i];
        auto& o = output[i];
        
        o.set_detail(item.detail.realize(context, state));
        o.set_tooltip(item.tooltip.realize(context, state));
        o.set_name(item.name.realize(context, state));
        if(not std::holds_alternative<bool>(item.disabled_template))
            o.set_disabled(Meta::fromStr<bool>(std::get<pattern::UnresolvedStringPattern>(item.disabled_template).realize(context, state)));
    }
    o.to<ScrollableList<DetailTooltipItem>>()->set_items(std::move(output));
    return false;
}

bool HashedObject::update_loops(GUITaskQueue_t* gui, uint64_t, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state)
{
    bool dirty = false;
    bool error = false;
    
    auto &obj = std::get<LoopBody>(object);
    if (not obj._state) {
        obj._state = std::make_unique<State>();
        obj._state->_current_object_handler = state._current_object_handler;
    }
    //obj._state->_variable_values.clear();
    
    PreVarProps ps = extractControls(obj.variable);
    auto props = ps.parse(context, state);

    if(context.has(props.name)) {
        if(context.variable(props.name)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
            auto& vector = context.variable(props.name)->value<std::vector<std::shared_ptr<VarBase_t>>&>(props);
            
            //IndexScopeHandler handler{state._current_index};
            if(vector != obj.cache) {
                std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
                ptrs.resize(vector.size());
                
                //State &state = *obj._state;
                Context tmp = context;
                //obj._state->_current_index = {};
                //obj._state->_collectors->objects.clear();
                
                size_t i = 0;
                for(auto &v : vector) {
                    //auto previous = (*obj._state)._variable_values;
                    tmp.variables["i"] = v;
                    
                    // try to reuse existing objects first:
                    Layout::Ptr ptr = ptrs.at(i);
                    if(not ptr) {
                        //Print("Making new ", hash, " child.");
                        ptr = parse_object(gui, obj.child, tmp, *obj._state, context.defaults);
                    } //else
                        //obj._state->_current_index.inc();

                    if(ptr) {
                        if(DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state)) {
                            dirty = true;
                        }
                    }

                    ptrs.at(i) = std::move(ptr);

                    //(*obj._state)._variable_values = std::move(previous);

                    ++i;
                    if (i >= 5000) {
                        break;
                    }
                }
                
                ptrs.resize(i);
                o.to<Layout>()->set_children(std::move(ptrs));
                obj.cache = vector;
                
            } else {
                //State &state = *obj._state;
                Context tmp = context;
                for(size_t i=0; i<obj.cache.size() && i < 5000; ++i) {
                    //auto previous = (*obj._state)._variable_values;
                    tmp.variables["i"] = obj.cache[i];
                    auto& p = o.to<Layout>()->objects().at(i);
                    if(p) {
                        if(DynamicGUI::update_objects(gui, g, p, tmp, *obj._state)) {
                            //Print("Changed content");
                            dirty = true;
                        }
                    }
                    //p->parent()->stage()->Print(nullptr);
                    //obj._state->_variable_values = std::move(previous);
                }
            }
        }
        else if(context.variable(props.name)->is_vector()
                || (context.variable(props.name)->is<sprite::Map&>()
                    && not props.subs.empty()
                    && context.variable(props.name)->value<sprite::Map&>(props).has(props.subs.front())
                    && context.variable(props.name)->value<sprite::Map&>(props).at(props.subs.front()).get().is_array()))
        {
            auto str = context.variable(props.name)->value_string(props);
            auto vector = util::parse_array_parts(util::truncate(str));
            
            //obj._state->_collectors->objects.clear();
            
            Context tmp = context;
            std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
            if(ptrs.size() != min(5000u, vector.size()))
                dirty = true;
            ptrs.resize(vector.size());
            
            size_t i = 0;
            for(auto &v : vector) {
                //auto previous = (*obj._state)._variable_values;
                
                tmp.variables["index"] = std::unique_ptr<VarBase_t>{
                    new Variable([i](const VarProps&) -> size_t {
                        return i;
                    })
                };
                tmp.variables["i"] = std::unique_ptr<VarBase_t>{
                    new Variable([v](const VarProps&) -> std::string {
                        return v;
                    })
                };
                
                // try to reuse existing objects first:
                Layout::Ptr &ptr = ptrs.at(i);
                if(not ptr) {
                    ptr = parse_object(gui, obj.child, tmp, *obj._state, context.defaults);
                    dirty = true;
                }
                if(ptr) {
                    if(DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state)) {
                        dirty = true;
                    } else if(ptr && ptr->is_dirty()) {
                        dirty = true;
                    }
                }
                //obj._state->_variable_values = std::move(previous);

                ++i;
                if (i >= 5000) {
                    break;
                }
            }
            
            ptrs.resize(i);
            o.to<Layout>()->set_children(ptrs);
            
            if(dirty) {
                //Print(o.to<Layout>(), " is dirty.");
                o.to<Layout>()->set_layout_dirty();
                o.to<Layout>()->update();
            }
            //o.to<Layout>()->set_children(ptrs);
        }
        else if(context.variable(props.name)->is<std::string>()) {
            auto str = context.variable(props.name)->value<std::string>(props);
            if(utils::beginsWith(str, '[')
               && utils::endsWith(str, ']'))
            {
                std::vector<std::string_view> vector;
                
                try {
                    vector = util::parse_array_parts(util::truncate(std::string_view(str)));
                } catch(const std::exception& ex) {
                    error = true;
                }
                
                std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
                if(ptrs.size() != min(5000u, vector.size()))
                    dirty = true;
                ptrs.resize(vector.size());
                
                Context tmp = context;
                size_t i = 0;
                for(auto &v : vector) {
                    tmp.variables["index"] = std::unique_ptr<VarBase_t>{
                        new Variable([i](const VarProps&) -> size_t {
                            return i;
                        })
                    };
                    tmp.variables["i"] = std::unique_ptr<VarBase_t>{
                        new Variable([v = std::string(v)](const VarProps&) -> std::string {
                            return v;
                        })
                    };
                    
                    // try to reuse existing objects first:
                    Layout::Ptr &ptr = ptrs.at(i);
                    if(not ptr) {
                        ptr = parse_object(gui, obj.child, tmp, *obj._state, context.defaults);
                        dirty = true;
                    }
                    if(ptr) {
                        if(DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state)) {
                            dirty = true;
                        } else if(ptr && ptr->is_dirty()) {
                            dirty = true;
                        }
                    }
                    
                    ++i;
                    if (i >= 5000) {
                        break;
                    }
                }
                
                ptrs.resize(i);
                o.to<Layout>()->set_children(std::move(ptrs));
                
                if(dirty) {
                    //Print(o.to<Layout>(), " is dirty.");
                    o.to<Layout>()->set_layout_dirty();
                    o.to<Layout>()->update();
                }
                
            } else {
                error = true;
            }
        }
        else {
            error = true;
        }
    }
    else error = true;
    
    if(error) {
        auto ptrs = std::vector<Layout::Ptr>();
        auto text = settings::htmlify("Invalid loop for "+ context.variable(props.name)->value_string(props)+ " in "+glz::write_json(obj.child).value_or("null"));
        ptrs.push_back(Layout::Make<StaticText>(attr::Str{text}));
        o.to<Layout>()->set_children(std::move(ptrs));
        o.to<Layout>()->set_layout_dirty();
        o.to<Layout>()->update();
    }
    
    return dirty;
}

void State::register_delete_callback(const std::shared_ptr<HashedObject>& ptr, const Layout::Ptr &o)
{
    o->clear_delete_handlers();
    
    o->on_delete([fn = std::weak_ptr(_collectors), weak = std::weak_ptr(ptr)](){
        /// first check if the object still exists!
        auto variant_lock = weak.lock();
        if(not variant_lock)
            return; // if not then we arent supposed to be here anyway
        
        /// variant will stay alive
        auto ptr = fn.lock();
        if(auto collector = fn.lock();
           collector)
        {
            /// mark variant for deletion since we deleted its
            /// main object:
            collector->dealloc(variant_lock->hash);
        }
    });
}

std::shared_ptr<HashedObject> State::register_variant(size_t hash, const Layout::Ptr& o) {
    if(_collectors->objects.contains(hash)) {
        if(std::holds_alternative<std::monostate>(_collectors->objects.at(hash)->object)) {
            auto& ptr = _collectors->objects.at(hash);
            if(o)
                register_delete_callback(ptr, o);
            return ptr;
        } else {
            Print("Evicting object ", hash, " from cache.");
        }
    }
    
    auto ptr = std::make_shared<HashedObject>();
    ptr->hash = hash;
    _collectors->objects[hash] = ptr;
    if(o)
        register_delete_callback(ptr, o);
    return ptr;
}

std::shared_ptr<HashedObject> State::register_monostate(size_t hash, const Layout::Ptr& ptr) {
    return register_variant(hash, ptr);
}

std::shared_ptr<HashedObject> State::get_monostate(size_t hash, const Layout::Ptr& ptr) {
    if(auto it = _collectors->objects.find(hash);
       it != _collectors->objects.end())
    {
        return it->second;
    }
    
    return register_variant(hash, ptr);
}

void State::register_pattern(size_t hash, const std::string& name, Pattern &&pattern) {
    auto it = _collectors->objects.find(hash);
    if(it == _collectors->objects.end()) {
        _collectors->objects[hash] = std::make_shared<HashedObject>();
    }
    
    _collectors->objects.at(hash)->patterns[name] = std::move(pattern);
}

std::optional<Pattern*> State::get_pattern(size_t hash, const std::string& name) {
    auto it = _collectors->objects.find(hash);
    if(it == _collectors->objects.end())
        return std::nullopt;
    
    if(auto kit = it->second->patterns.find(name);
       kit != it->second->patterns.end())
    {
        return &kit->second;
    }
    
    return std::nullopt;
}

std::shared_ptr<Drawable> State::named_entity(std::string_view name) {
    auto lock = _current_object_handler.lock();
    if(not lock)
        return nullptr;
    
    return lock->retrieve_named(name);
}

constexpr inline bool contains_bad_variable(std::string_view name) {
    return is_in(name, "i", "index", "hovered", "selected")
            || utils::beginsWith(name, "i.")
            || utils::contains(name, "{hovered}")
            || utils::contains(name, "{selected}")
            || utils::contains(name, "{i.")
            || utils::contains(name, "{i}")
            || utils::contains(name, "{index}");
}

std::optional<std::string_view> State::cached_variable_value(std::string_view name) const
{
    //if(contains_bad_variable(name))
    //    return std::nullopt;
    
    auto lock = _current_object_handler.lock();
    if(not lock)
        return std::nullopt;
    
    return lock->get_variable_value(name);
}

void State::set_cached_variable_value(std::string_view name, std::string_view value) {
    if(contains_bad_variable(name))
        return;
    
    auto lock = _current_object_handler.lock();
    if(not lock)
        return;
    
    lock->set_variable_value(name, value);
}

/*State::State(const State& other)
    : patterns(other.patterns),
      _var_cache(other._var_cache),
      _named_entities(other._named_entities),
      _current_object_handler(other._current_object_handler),
      _last_settings_box(other._last_settings_box),
      _settings_was_selected(other._settings_was_selected),
      _current_index(other._current_index),
      _collectors(std::make_shared<Collectors>(*other._collectors))
{
    for(auto &[k, body] : other.loops) {
        loops[k] = {
            .variable = body.variable,
            .child = body.child,
            //.state = std::make_unique<State>(*body.state),
            .cache = body.cache
        };
    }
    for(auto &[k, body] : other.lists) {
        lists[k] = {
            .variable = body.variable,
            .item = body.item,
            .state = std::make_unique<State>(*body.state),
            .cache = body.cache,
            .on_select_actions = {}
        };
    }
    for (auto& [k, body] : other.manual_lists) {
        manual_lists[k] = {
            .items = body.items
        };
    }
}*/

void State::Collectors::dealloc(size_t hash) {
    /*if(ifs.contains(hash)) {
        ifs.erase(hash);
    }
    if(_timers.contains(hash)) {
        _timers.erase(hash);
    }
    if(_customs.contains(hash)) {
        _customs.erase(hash);
    }
    if(_customs_cache.contains(hash)) {
        _customs_cache.erase(hash);
    }*/
    if(objects.contains(hash))
        objects.erase(hash);
}

}
