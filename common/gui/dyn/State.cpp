#include "State.h"
#include <gui/LabeledField.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>
#include <gui/DynamicGUI.h>

namespace cmn::gui::dyn {


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
            } else if(variable.is<sprite::Map&>()) {
                auto& tmp = variable.value<sprite::Map&>({});
                if(modifiers.subs.empty())
                    throw U_EXCEPTION("sprite::Map selector is empty, need subvariable doing ", word,".sub");
                
                auto ref = tmp[modifiers.subs.front()];
                if(not ref.get().valid()) {
                    FormatWarning("Retrieving invalid property (", modifiers, ") from map with keys ", tmp.keys(), ".");
                    return false;
                }
                //Print(ref.get().name()," = ", ref.get().valueString(), " with ", ref.get().type_name());
                if constexpr(std::same_as<T, bool>) {
                    if(ref.is_type<bool>())
                        return ref.value<bool>();
                    else
                        return convert_to_bool(ref.get().valueString());
                    
                } else if(ref.template is_type<T>()) {
                    return ref.get().template value<T>();
                } else if(ref.is_type<std::string>()) {
                    return not ref.get().value<std::string>().empty();
                } else if(ref.is_type<file::Path>()) {
                    return not ref.get().value<file::Path>().empty();
                }
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
    
    if(not o.is<Layout>()) {
        state._collectors->dealloc(obj._assigned_hash);
        //it = state._collectors->ifs.erase(it);
        return true;
    }
    try {
        auto res = resolve_variable_type<bool>(obj.variable, context, state);
        auto last_condition = (uint64_t)o->custom_data("last_condition");
        
        //IndexScopeHandler handler{state._current_index};
        if(not res) {
            if(obj._if) {
                obj._if = nullptr;
                o.to<Layout>()->set_children({});
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
                    
                    o.to<Layout>()->set_children({obj._else});
                    
                    if(ref != obj._else) {
                        FormatWarning("Differs! ", *ref, " vs. ", *obj._else);
                        return true;
                    }
                }
                
                if(DynamicGUI::update_objects(gui, g, obj._else, context, *obj._state)) {
                    FormatWarning("Object changed after update");
                }
                
            } else {
                if(o->is_displayed()) {
                    o->set_is_displayed(false);
                    o.to<Layout>()->clear_children();
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
                o.to<Layout>()->set_children({});
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
                o.to<Layout>()->set_children({obj._if});
            }
            
            auto ref = obj._if;
            if(DynamicGUI::update_objects(gui, g, obj._if, context, *obj._state)) {
                FormatWarning("Object changed after update.");
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

bool HashedObject::update_patterns(GUITaskQueue_t* gui, uint64_t hash, Layout::Ptr &o, const Context &context, State &state) {
    bool changed{false};
    
    LabeledField *field{_text_field.get()};
    //Print("Found pattern ", pattern, " at index ", hash);
    
    if(patterns.contains("var")) {
        try {
            auto var = parse_text(patterns.at("var"), context, state);
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
    
    if(patterns.contains("fill")) {
        try {
            auto fill = resolve_variable_type<Color>(patterns.at("fill"), context, state);
            LabeledField::delegate_to_proper_type(FillClr{fill}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if(patterns.contains("color")) {
        try {
            auto clr = resolve_variable_type<Color>(patterns.at("color"), context, state);
            LabeledField::delegate_to_proper_type(TextClr{clr}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if(patterns.contains("line")) {
        try {
            auto line = resolve_variable_type<Color>(patterns.at("line"), context, state);
            LabeledField::delegate_to_proper_type(LineClr{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("placeholder")) {
        try {
            auto placeholder = parse_text(patterns.at("placeholder"), context, state);
            LabeledField::delegate_to_proper_type(Placeholder_t{placeholder}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("pad")) {
        try {
            auto line = resolve_variable_type<Bounds>(patterns.at("pad"), context, state);
            LabeledField::delegate_to_proper_type(Margins{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("alpha")) {
        try {
            auto line = resolve_variable_type<float>(patterns.at("alpha"), context, state);
            LabeledField::delegate_to_proper_type(Alpha{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("pos")) {
        try {
            auto str = parse_text(patterns.at("pos"), context, state);
            ptr->set_pos(Meta::fromStr<Vec2>(str));
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("scale")) {
        try {
            ptr->set_scale(Meta::fromStr<Vec2>(parse_text(patterns.at("scale"), context, state)));
            //o->set_scale(resolve_variable_type<Vec2>(patterns.at("scale"), context));
            //Print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if(patterns.contains("origin")) {
        try {
            ptr->set_origin(Meta::fromStr<Vec2>(parse_text(patterns.at("origin"), context, state)));
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("size")) {
        try {
            auto& size = patterns.at("size");
            if(size.original == "auto")
            {
                if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
            } else {
                ptr->set_size(Meta::fromStr<Size2>(parse_text(size, context, state)));
                //o->set_size(resolve_variable_type<Size2>(size, context));
            }
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("max_size")) {
        try {
            auto line = Meta::fromStr<Size2>(parse_text(patterns.at("max_size"), context, state));
            LabeledField::delegate_to_proper_type(SizeLimit{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if(patterns.contains("preview_max_size")) {
        try {
            auto line = Meta::fromStr<Size2>(parse_text(patterns.at("preview_max_size"), context, state));
            LabeledField::delegate_to_proper_type(SizeLimit{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if (patterns.contains("item_line")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("item_line"), context, state));
            LabeledField::delegate_to_proper_type(ItemBorderColor_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if (patterns.contains("item_color")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("item_color"), context, state));
            LabeledField::delegate_to_proper_type(ItemColor_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if (patterns.contains("label_line")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("label_line"), context, state));
            LabeledField::delegate_to_proper_type(LabelBorderColor_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if (patterns.contains("label_fill")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("label_fill"), context, state));
            LabeledField::delegate_to_proper_type(LabelColor_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if(patterns.contains("list_size")) {
        try {
            auto line = Meta::fromStr<Size2>(parse_text(patterns.at("list_size"), context, state));
            LabeledField::delegate_to_proper_type(ListDims_t{line}, ptr);
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    if (patterns.contains("list_line")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("list_line"), context, state));
            LabeledField::delegate_to_proper_type(ListLineClr_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }

    if (patterns.contains("list_fill")) {
        try {
            auto line = Meta::fromStr<Color>(parse_text(patterns.at("list_fill"), context, state));
            LabeledField::delegate_to_proper_type(ListFillClr_t{ line }, ptr);

        }
        catch (const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    if(patterns.contains("text")) {
        try {
            auto text = Str{parse_text(patterns.at("text"), context, state)};
            LabeledField::delegate_to_proper_type(text, ptr);
        } catch(const std::exception& e) {
            FormatError("Error parsing context ", patterns.at("text"),": ", e.what());
        }
    }
    
    if(patterns.contains("radius")) {
        try {
            auto text = Radius{Meta::fromStr<float>(parse_text(patterns.at("radius"), context, state))};
            LabeledField::delegate_to_proper_type(text, ptr);
        } catch(const std::exception& e) {
            FormatError("Error parsing context ", patterns.at("radius"),": ", e.what());
        }
    }

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
    if(context.has(obj.variable)) {
        if(context.variable(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
            
            auto& vector = context.variable(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>({});
            
            IndexScopeHandler handler{state._current_index};
            //if(vector != obj.cache) {
            std::vector<DetailItem> ptrs;
            //obj.cache = vector;
            obj._state = std::make_unique<State>();
            obj._state->_current_object_handler = state._current_object_handler;
            
            Context tmp = context;
            
            size_t index=0;
            auto convert_to_item = [&gc = context, &index, &obj](sprite::Map&, const glz::json_t& item_template, Context& context, State& state) -> DetailItem
            {
                DetailItem item;
                if(item_template.contains("text") && item_template["text"].is_string()) {
                    item.set_name(parse_text(item_template["text"].get<std::string>(), context, state));
                }
                if(item_template.contains("detail") && item_template["detail"].is_string()) {
                    item.set_detail(parse_text(item_template["detail"].get<std::string>(), context, state));
                }
                if(item_template.contains("action") && item_template["action"].is_string()) {
                    auto action = PreAction::fromStr(parse_text(item_template["action"].get<std::string>(), context, state));
                    
                    if(not obj.on_select_actions.contains(index)
                       || std::get<0>(obj.on_select_actions.at(index)) != action.name)
                    {
                        obj.on_select_actions[index] = std::make_tuple(
                            index, [&gc = gc, index = index, action = action, context](){
                                Print("Clicked item at ", index, " with action ", action);
                                State state;
                                Action _action = action.parse(context, state);
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
            
            for(auto &v : vector) {
                //auto previous = state._variable_values;
                tmp.variables["i"] = v;
                try {
                    auto &ref = v->value<sprite::Map&>({});
                    auto item = convert_to_item(ref, obj.item, tmp, state);
                    ptrs.emplace_back(std::move(item));
                    ++index;
                } catch(const std::exception& ex) {
                    FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item), " and type ", v->class_name());
                }
                //state._variable_values = std::move(previous);
            }
            
            o.to<ScrollableList<DetailItem>>()->set_items(ptrs);
        }
    }
    return false;
}

bool HashedObject::update_manual_lists(GUITaskQueue_t *, uint64_t, DrawStructure &, const Layout::Ptr &o, const Context &context, State &state) {
    ManualListContents &body = std::get<ManualListContents>(object);
    auto items = body.items;
    for (auto& i : items) {
        i.set_detail(parse_text(i.detail(), context, state));
        i.set_name(parse_text(i.name(), context, state));
    }
    o.to<ScrollableList<DetailItem>>()->set_items(items);
    return false;
}

bool HashedObject::update_loops(GUITaskQueue_t* gui, uint64_t, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state)
{
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

                    if(ptr)
                        DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state);

                    ptrs.at(i) = std::move(ptr);

                    //(*obj._state)._variable_values = std::move(previous);

                    ++i;
                    if (i >= 500) {
                        break;
                    }
                }
                
                ptrs.resize(i);
                o.to<Layout>()->set_children(std::move(ptrs));
                obj.cache = vector;
                
            } else {
                //State &state = *obj._state;
                Context tmp = context;
                for(size_t i=0; i<obj.cache.size() && i < 500; ++i) {
                    //auto previous = (*obj._state)._variable_values;
                    tmp.variables["i"] = obj.cache[i];
                    auto& p = o.to<Layout>()->objects().at(i);
                    if(p) {
                        DynamicGUI::update_objects(gui, g, p, tmp, *obj._state);
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
                Layout::Ptr ptr = ptrs.at(i);
                if(not ptr) {
                    ptr = parse_object(gui, obj.child, tmp, *obj._state, context.defaults);
                }

                if(ptr) {
                    DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state);
                }

                ptrs.at(i) = std::move(ptr);

                //obj._state->_variable_values = std::move(previous);

                ++i;
                if (i >= 500) {
                    break;
                }
            }
            
            ptrs.resize(i);
            
            /*size_t i=0;
            for(auto &v : vector) {
                auto previous = obj._state->_variable_values;
                //tmp.variables["i"] = v;
                tmp.variables["index"] = std::unique_ptr<VarBase_t>{
                    new Variable([i = i++](const VarProps&) -> size_t {
                        return i;
                    })
                };
                tmp.variables["i"] = std::unique_ptr<VarBase_t>{
                    new Variable([v](const VarProps&) -> std::string {
                        return v;
                    })
                };
                auto ptr = parse_object(gui, obj.child, tmp, *obj._state, context.defaults);
                if(ptr) {
                    DynamicGUI::update_objects(gui, g, ptr, tmp, *obj._state);
                }
                ptrs.push_back(ptr);
                obj._state->_variable_values = std::move(previous);
            }*/
            
            //o.to<Layout>()->set_children(ptrs);
            o.to<Layout>()->set_children(std::move(ptrs));
            //obj.cache = vector;
        }
    }
    
    return false;
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

std::optional<const Pattern*> State::get_pattern(size_t hash, const std::string& name) {
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
            || utils::contains(name, "{i}");
}

std::optional<std::string_view> State::cached_variable_value(std::string_view name) const
{
    if(contains_bad_variable(name))
        return std::nullopt;
    
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
