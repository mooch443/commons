#include "State.h"
#include <gui/LabeledField.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>
#include <gui/dyn/ResolveVariable.h>
#include <gui/dyn/Action.h>
#include <gui/DynamicGUI.h>
#include <gui/Passthrough.h>
#include <gui/types/TagList.h>
#include <misc/Path.h>
#include <misc/default_settings.h>
#include <gui/dyn/binders.h>

namespace cmn::gui::dyn {

static constexpr uint64_t max_displayed_objects = 10000u;

static std::optional<glz::json_t> access_loop_json_subfields(glz::json_t value, const std::vector<std::string>& subs) {
    for(const auto& sub : subs) {
        if(value.is_object()) {
            if(not value.contains(sub)) {
                return std::nullopt;
            }
            value = value[sub];
            
        } else if(value.is_array()) {
            uint16_t index{};
            try {
                index = Meta::fromStr<uint16_t>(sub);
            } catch(...) {
                return std::nullopt;
            }
            auto& array = value.get_array();
            if(index >= array.size()) {
                return std::nullopt;
            }
            value = array[index];
            
        } else {
            return std::nullopt;
        }
    }
    
    return value;
}

static std::shared_ptr<CurrentObjectHandler> ensure_current_object_handler(State& state) {
    auto handler = state._current_object_handler.lock();
    if(not handler) {
        handler = std::make_shared<CurrentObjectHandler>();
        state._current_object_handler = std::weak_ptr(handler);
    }
    return handler;
}

static std::string loop_item_name(const LoopBody& loop) {
    return loop.item_name.empty() ? std::string("i") : loop.item_name;
}

static bool can_parse_as_simple_loop_variable(std::string_view variable) noexcept {
    variable = utils::trim(variable);
    if(variable.empty())
        return false;

    if(variable.front() == '{') {
        if(variable.back() != '}')
            return false;
        variable = variable.substr(1, variable.size() - 2);
        variable = utils::trim(variable);
        if(variable.empty())
            return false;
    }

    bool has_name_character = false;
    for(const char c : variable) {
        if(c == ':' || c == '{' || c == '}' || c == '[' || c == ']' || c == '\'' || c == '"')
            return false;
        if(!std::isspace(static_cast<unsigned char>(c)) && c != '#' && c != '.')
            has_name_character = true;
    }

    return has_name_character;
}

// Shared by `list` and `each`: accepts raw JSON arrays, vectors of JSON
// values, and variables that expose a to_json() array through VarBase.
static std::optional<std::vector<glz::json_t>> json_array_from_variable(
    const std::shared_ptr<VarBase_t>& variable,
    const VarProps& props)
{
    if(variable->is<glz::json_t>()) {
        auto object = variable->access_value<glz::json_t>([&props](auto&& value) {
            return access_loop_json_subfields(value, props.subs);
        }, props);
        if(object && object->is_array())
            return object->get_array();
        return std::vector<glz::json_t>{};
    }

    if(variable->is<std::vector<glz::json_t>>()) {
        return variable->access_value<std::vector<glz::json_t>>([](auto&& value) {
            return std::vector<glz::json_t>(value.begin(), value.end());
        }, props);
    }

    return std::nullopt;
}

static std::vector<std::string> string_values_from_source(
    const glz::json_t& source,
    const Context& context,
    State& state)
{
    glz::json_t resolved = source;
    if(source.is_string()) {
        const auto spec = source.get<std::string>();
        try {
            auto prepared = extractControls(spec);
            auto props = prepared.parse(context, state);
            if(context.has(props.name, state)) {
                auto variable = context.variable(props.name, state);
                if(auto values = json_array_from_variable(variable, props); values.has_value()) {
                    glz::json_t::array_t array;
                    array.assign(values->begin(), values->end());
                    resolved = std::move(array);
                } else if(variable->value_string) {
                    glz::json_t::array_t array;
                    for(auto& value : Meta::fromStr<std::vector<std::string>>(variable->value_string(props)))
                        array.emplace_back(std::move(value));
                    resolved = std::move(array);
                } else {
                    resolved = glz::json_t::array_t{};
                }
            } else {
                const auto text = parse_text(spec, context, state);
                glz::json_t parsed;
                if(glz::read_json(parsed, text) == glz::error_code::none) {
                    resolved = std::move(parsed);
                } else {
                    glz::json_t::array_t array;
                    for(auto& value : Meta::fromStr<std::vector<std::string>>(text))
                        array.emplace_back(std::move(value));
                    resolved = std::move(array);
                }
            }
        } catch(const std::exception& e) {
            FormatWarning("Cannot resolve string-list source ", spec, ": ", e.what());
            resolved = glz::json_t::array_t{};
        }
    }

    std::vector<std::string> values;
    if(not resolved.is_array())
        return values;

    values.reserve(resolved.size());
    for(const auto& item : resolved.get_array()) {
        if(item.is_string())
            values.emplace_back(item.get<std::string>());
    }
    return values;
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
    } else if(std::holds_alternative<TagListContents>(object)) {
        (void)update_tag_lists(gui, hash, g, o, context, state);
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
                if(not obj._state)
                    obj._state = std::make_unique<State>();
                State& _state = *obj._state;
                _state._current_object_handler = state._current_object_handler;
                
                if(not obj._else) {
                    for(auto &[n, p] : patterns) {
                        //obj._state->_collectors->objects[hash][n] = Pattern{p};
                        _state.pattern().set(1, n, Pattern{p});
                    }
                    
                    _state._collectors->objects.clear();
                    _state._current_index = {};
                    //obj._state->_variable_values.clear();
                    
                    obj._else = parse_object(gui, obj.__else.get_object(), context, _state, context.defaults);
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
                
                if(DynamicGUI::update_objects(gui, g, obj._else, context, _state)) {
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
            
            if(not obj._state)
                obj._state = std::make_unique<State>();
            State& _state = *obj._state;
            _state._current_object_handler = state._current_object_handler;
            
            if(not obj._if) {
                for(auto &[n, p] : patterns) {
                    //obj._state->_collectors->objects[hash][n] = Pattern{p};
                    _state.pattern().set(1, n, Pattern{p});
                }
                
                _state._collectors->objects.clear();
                _state._current_index = {};
                
                obj._if = parse_object(gui, obj.__if.get_object(), context, _state, context.defaults);
            } //else
                //state._current_index.inc();
            
            if(last_condition != 2) {
                o->set_is_displayed(true);
                o->add_custom_data("last_condition", (void*)2);
                pass->set_object(obj._if);
            }
            
            auto ref = obj._if;
            if(DynamicGUI::update_objects(gui, g, obj._if, context, _state)) {
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
    
    LabeledField *field{_value_box.get()};
    const auto patterns_end = patterns.end();
    //Print("Found pattern ", pattern, " at index ", hash);
    
    if(patterns.contains("var")) {
        try {
            auto var = patterns.at("var").realize(context, state);
            //auto var = parse_text(patterns.at("var"), context, state);
            if(_value_box) {
                auto str = _value_box->ref().get().valueString();
                if(not _var_cache.has_value()
                   || _var_cache->_var != var)
                {
                    if(_var_cache.has_value()) {
                        Print("Need to change ", _var_cache->_value," => ", str, " and ", _var_cache->_var, " => ", var);
                        
                        //! replace old object (also updates the cache)
                        o = parse_object(gui, _var_cache->_obj, context, state, context.defaults, hash);
                        
                        field = _value_box.get();
                        
                        auto ptr = field->representative();
                        _var_cache->_cached_ptr = std::weak_ptr(ptr.get_smart());
                        
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
        
        if(_var_cache.has_value())
        {
            auto lock = _var_cache->_cached_ptr.lock();
            if(not lock) {
                _var_cache->_cached_ptr = std::weak_ptr(ptr.get_smart());

            } else if(lock.get() != ptr.get()) {
                changed = true;
                
                try {
                    if(not field->ref().name().empty()) {
#ifndef NDEBUG
                        FormatWarning("* renew item ", hex(o.get()), " and value_box = ", hex(_value_box.get()), " for ptr ", hex(ptr.get()));
#endif
                        /// workaround currently necessary maybe
                        /// regenerating the entire subtree so we
                        /// update immediately
                        o = parse_object(gui, _var_cache->_obj, context, state, context.defaults, hash);
                        
                        auto hashed = state.get_monostate(hash, o);

                        field = _value_box.get();
                        ptr = field->representative();
#ifndef NDEBUG
                        FormatWarning("* => value_box = ", hex(_value_box.get()), " for ptr ", hex(ptr.get()));
#endif
                    }
                    
                    _var_cache->_cached_ptr = std::weak_ptr(ptr.get_smart());
                } catch(std::exception& e) {
                    FormatError("Error parsing context; ", patterns, ": ", e.what());
                }
            }
            
        } else {
            throw std::runtime_error("VarCache was not initialized.");
        }
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
            auto trimmed = utils::trim(std::string_view(text));
            if constexpr(std::same_as<SourceType, Vec2> || std::same_as<SourceType, Size2>) {
                // Ignore transient unresolved values instead of coercing to zero.
                if(trimmed.empty() || trimmed == "null" || utils::contains(trimmed, "null"))
                    return;
            }
            auto fill = Meta::fromStr<SourceType>(text);
            if(field)
                field->set(TargetType{fill});
            else
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

    if(ptr.is<Line>()) {
        auto line_ptr = ptr.to<Line>();
        auto check_line_field = [&]<typename SourceType>(std::string_view name, auto&& apply) -> bool {
            auto it = patterns.find(name);
            if(it == patterns_end)
                return false;

            try {
                auto text = it->second.realize(context, state);
                auto trimmed = utils::trim(std::string_view(text));
                if constexpr(std::same_as<SourceType, Vec2> || std::same_as<SourceType, Size2>) {
                    if(trimmed.empty() || trimmed == "null" || utils::contains(trimmed, "null"))
                        return true;
                }
                apply(Meta::fromStr<SourceType>(text));
            } catch(const std::exception& e) {
                FormatError("Error parsing line context; ", patterns, ": ", e.what());
            }
            return true;
        };

        check_line_field.operator()<Color>("color", [&](const auto& value) {
            line_ptr->set(LineClr{value});
        });
        check_line_field.operator()<Float2_t>("thickness", [&](const auto& value) {
            line_ptr->set(Line::Thickness_t{value});
        });

        if(not check_line_field.operator()<std::vector<Vec2>>("points", [&](const auto& value) {
            line_ptr->set(Line::Points_t{value});
        })) {
            auto points = line_ptr->raw_points();
            if(points.size() < 2) {
                points = {
                    Vertex(Vec2{}, LineClr{line_ptr->line_clr()}),
                    Vertex(Vec2{}, LineClr{line_ptr->line_clr()})
                };
            }

            bool changed_points = false;
            const auto bounds_pos = line_ptr->bounds().pos();
            auto from = bounds_pos + points.front().position();
            auto to = bounds_pos + points.at(1).position();
            auto color = line_ptr->line_clr();
            changed_points |= check_line_field.operator()<Vec2>("from", [&](const auto& value) {
                from = value;
            });
            changed_points |= check_line_field.operator()<Vec2>("to", [&](const auto& value) {
                to = value;
            });

            if(changed_points) {
                line_ptr->set(Line::Point_t{from}, Line::Point_t{to}, LineClr{color}, Line::Thickness_t{line_ptr->thickness()});
            }
        }
    }
    
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
    check_field.operator()<Bounds, OuterPadding>("outer_pad");
    check_field.operator()<Float2_t, Alpha>("alpha");
    if(auto it = patterns.find("pos");
       it != patterns_end)
    {
        try {
            auto text = it->second.realize(context, state);
            auto trimmed = utils::trim(std::string_view(text));
            if(!(trimmed.empty() || trimmed == "null" || utils::contains(trimmed, "null"))) {
                auto value = Meta::fromStr<Vec2>(text);
                if(not (o->draggable() && o->being_dragged())) {
                    if(field)
                        o->set(Loc{value});
                    else
                        LabeledField::delegate_to_proper_type(Loc{value}, ptr);
                }
            }
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
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
            auto trimmed = utils::trim(std::string_view(text));
            if(!(trimmed.empty() || trimmed == "null" || utils::contains(trimmed, "null"))) {
                if(text == "auto") {
                    if(ptr.is<Layout>()) ptr.to<Layout>()->auto_size();
                    else FormatExcept("pattern for size should only be auto for layouts, not: ", *ptr);
                } else {
                    //LabeledField::delegate_to_proper_type(Size{Meta::fromStr<Size2>(text)}, ptr);
                    if(field)
                        field->set(Size{Meta::fromStr<Size2>(text)});
                    else
                        ptr->set(Size{Meta::fromStr<Size2>(text)});
                }
            }
            
        } catch(const std::exception& e) {
            FormatError("Error parsing context; ", patterns, ": ", e.what());
        }
    }
    
    check_field.operator()<Size2, SizeLimit>("max_size");
    check_field.operator()<Size2, SizeLimit>("preview_max_size");
    check_field.operator()<bool, TagList::AllowNew_t>("allow_new");
    check_field.operator()<float, TagList::MatchThreshold_t>("match_threshold");
    check_field.operator()<Vec2, ItemPadding_t>("item_pad");
    check_field.operator()<Color, ItemFillClr_t>("item_fill");
    check_field.operator()<Color, ItemLineColor_t>("item_line");
    check_field.operator()<Color, ItemTextClr_t>("item_color");
    check_field.operator()<CornerFlags_t, CornerFlags_t>("item_corners");
    check_field.operator()<Size2, LabelDims_t>("input_size");
    check_field.operator()<Size2, ListDims_t>("input_list_size");
    check_field.operator()<std::string, Placeholder_t>("input_placeholder");
    check_field.operator()<Color, LabelFillClr_t>("input_fill");
    check_field.operator()<Color, LabelLineColor_t>("input_line");
    check_field.operator()<Color, LabelColor_t>("input_color");
    check_field.operator()<Color, LabelLineColor_t>("label_line");
    check_field.operator()<Color, LabelFillClr_t>("label_fill");
    check_field.operator()<Color, LabelColor_t>("label_color");
    check_field.operator()<Size2, LabelDims_t>("label_size");
    check_field.operator()<CornerFlags, LabelCornerFlags>("label_corners");
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
    auto convert_to_item = [&context, &index, &obj](ListContents::ItemTemplate& item_template, State& state) -> DetailTooltipItem
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
                auto fn = bind_with_state(state._current_object_handler, context, [action, index](const Context& context, State& state) {
                    Print("Clicked item at ", index, " with action ", action);
                    
                    Action _action = action.parse(context, state);
                    if(_action.parameters.empty())
                        _action.parameters = { Meta::toStr(index) };
                    if(auto it = context.actions.find(action.name); it != context.actions.end()) {
                        try {
                            it->second(_action);
                        } catch(const std::exception& ex) {
                            // pass
                        }
                    }
                });
                
                obj.on_select_actions[index] = std::make_tuple(index, std::move(fn));
            }
        }
        
        return item;
    };
    
    if(context.has(obj.variable, state)) {
        auto loop_variable = context.variable(obj.variable, state);
        if(loop_variable->is_strict<std::vector<std::shared_ptr<VarBase_t>>&>()) {
            auto& vector = loop_variable->value<std::vector<std::shared_ptr<VarBase_t>>&>({});
            
            IndexScopeHandler handler{state._current_index};
            //if(vector != obj.cache) {
            std::vector<DetailTooltipItem> ptrs;
            //obj.cache = vector;
#ifndef NDEBUG
            obj._state = std::make_unique<State>();
            obj._state->_current_object_handler = state._current_object_handler;
#endif
            
            auto scoped_handler = ensure_current_object_handler(state);
            auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});
            
            for(auto &v : vector) {
                auto scope = scoped_handler->scope();
                scope.set("i", v);
                scope.set("index", Meta::toStr(index));
                if(have_index)
                    scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                        return index;
                    }).second);
                
                try {
                    auto item = convert_to_item(obj.item, state);
                    ptrs.emplace_back(std::move(item));
                    ++index;
                } catch(const std::exception& ex) {
                    FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item).value_or("null"), " and type ", v->class_name());
                }
            }
            
            o.to<ScrollableList<DetailTooltipItem>>()->set_items(ptrs);
        }
        else if(loop_variable->is_vector()) {
            PreVarProps ps = extractControls(obj.variable);
            const auto props = ps.parse(context, state);
            
            auto vector = json_array_from_variable(loop_variable, props);
            
            IndexScopeHandler handler{state._current_index};
            std::vector<DetailTooltipItem> ptrs;
#ifndef NDEBUG
            obj._state = std::make_unique<State>();
            obj._state->_current_object_handler = state._current_object_handler;
#endif

            auto scoped_handler = ensure_current_object_handler(state);
            auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});

            for(auto& v : *vector) {
                auto scope = scoped_handler->scope();
                scope.set("i", VarFunc("i", [v](const VarProps&) -> glz::json_t {
                    return v;
                }).second);
                scope.set("index", VarFunc("index", [index](const VarProps&) -> size_t {
                    return index;
                }).second);
                if(have_index)
                    scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                        return index;
                    }).second);

                try {
                    auto item = convert_to_item(obj.item, state);
                    ptrs.emplace_back(std::move(item));
                    ++index;
                } catch(const std::exception&) {
                    FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item).value_or("null"), " and type json.");
                }
            }

            o.to<ScrollableList<DetailTooltipItem>>()->set_items(ptrs);
        }
        else {
            Print("Variable ", obj.variable, " has unknown type ", loop_variable->class_name(),".");
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
#ifndef NDEBUG
        obj._state = std::make_unique<State>();
        obj._state->_current_object_handler = state._current_object_handler;
#endif
        
        auto scoped_handler = ensure_current_object_handler(state);
        for(auto &v : vector) {
            auto scope = scoped_handler->scope();
            scope.set("i", VarFunc("i", [v](const VarProps&) -> std::string{
                return v;
            }).second);
            
            try {
                auto item = convert_to_item( obj.item, state);
                ptrs.emplace_back(std::move(item));
                ++index;
            } catch(const std::exception& ex) {
                FormatExcept("Cannot create list items for template: ", glz::write_json(obj.item).value_or("null"), " and type string.");
            }
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

bool HashedObject::update_tag_lists(GUITaskQueue_t*, uint64_t, DrawStructure&, const Layout::Ptr& o, const Context& context, State& state) {
    if(not o.is<TagList>())
        return false;

    const auto& contents = std::get<TagListContents>(object);
    auto tag_list = o.to<TagList>();
    tag_list->set_tags(string_values_from_source(contents.values, context, state));
    tag_list->set_catalog(string_values_from_source(contents.catalog, context, state));
    return false;
}

bool HashedObject::update_loops(GUITaskQueue_t* gui, uint64_t, DrawStructure &g, const Layout::Ptr &o, const Context &context, State &state)
{
    bool dirty = false;
    bool error = false;
    
    auto &obj = std::get<LoopBody>(object);
#ifndef NDEBUG
    /*if (not obj._state) {
        obj._state = std::make_unique<State>();
        obj._state->_current_object_handler = state._current_object_handler;
    }*/
#endif
    //obj._state->_variable_values.clear();

    const auto item_name = loop_item_name(obj);

    auto generate_objects_from_string = [&](const std::string& str){
        if(not utils::beginsWith(str, '[')
           || not utils::endsWith(str, ']'))
        {
            return false;
        }

        std::vector<std::string_view> vector;

        try {
            vector = util::parse_array_parts(util::truncate(std::string_view(str)));
        } catch(const std::exception& ex) {
            return false;
        }

        std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
        if(ptrs.size() != min(max_displayed_objects, vector.size()))
            dirty = true;
        ptrs.resize(vector.size());

        auto scoped_handler = ensure_current_object_handler(state);
        auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});

        size_t i = 0;
        for(auto &v : vector) {
            auto scope = scoped_handler->scope();
            scope.set("index", VarFunc("index", [i](const VarProps&) -> size_t {
                return i;
            }).second);
            scope.set(item_name, VarFunc(item_name, [v = std::string(v)](const VarProps&) -> std::string {
                return v;
            }).second);
            if(have_index)
                scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                    return index;
                }).second);

            // try to reuse existing objects first:
            Layout::Ptr &ptr = ptrs.at(i);
            if(not ptr) {
                ptr = parse_object(gui, obj.child, context, state, context.defaults);
                dirty = true;
            }
            if(ptr) {
                if(DynamicGUI::update_objects(gui, g, ptr, context, state)) {
                    dirty = true;
                } else if(ptr && ptr->is_dirty()) {
                    dirty = true;
                }
            }

            ++i;
            if (i >= max_displayed_objects) {
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

        return true;
    };

    std::optional<VarProps> typed_props;
    if(can_parse_as_simple_loop_variable(obj.variable)) {
        auto props = extractControls(obj.variable).parse(context, state);
        if(context.has(props.name, state))
            typed_props = std::move(props);
    }

    if(typed_props.has_value()) {
        const auto& props = typed_props.value();
        auto loop_variable = context.variable(props.name, state);
        if(loop_variable->is_strict<std::vector<std::shared_ptr<VarBase_t>>&>()) {
            auto& vector = loop_variable->value<std::vector<std::shared_ptr<VarBase_t>>&>(props);
            
            //IndexScopeHandler handler{state._current_index};
            if(vector != obj.cache) {
                std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
                ptrs.resize(vector.size());
                
                //State &state = *obj._state;
                auto scoped_handler = ensure_current_object_handler(state);
                auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});
                //obj._state->_current_index = {};
                //obj._state->_collectors->objects.clear();
                
                size_t i = 0;
                for(auto &v : vector) {
                    auto scope = scoped_handler->scope();
                    scope.set(item_name, v);
                    scope.set("index", Meta::toStr(i));
                    if(have_index)
                        scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                            return index;
                        }).second);
                    
                    // try to reuse existing objects first:
                    Layout::Ptr ptr = ptrs.at(i);
                    if(not ptr) {
                        //Print("Making new ", hash, " child.");
                        ptr = parse_object(gui, obj.child, context, state, context.defaults);
                    } //else
                        //obj._state->_current_index.inc();

                    if(ptr) {
                        if(DynamicGUI::update_objects(gui, g, ptr, context, state)) {
                            dirty = true;
                        }
                    }

                    ptrs.at(i) = std::move(ptr);

                    ++i;
                    if (i >= max_displayed_objects) {
                        break;
                    }
                }
                
                ptrs.resize(i);
                o.to<Layout>()->set_children(std::move(ptrs));
                obj.cache = vector;
                
            } else {
                //State &state = *obj._state;
                auto scoped_handler = ensure_current_object_handler(state);
                auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});
                
                for(size_t i=0; i<obj.cache.size() && i < max_displayed_objects; ++i) {
                    auto scope = scoped_handler->scope();
                    scope.set(item_name, obj.cache[i]);
                    scope.set("index", Meta::toStr(i));
                    if(have_index)
                        scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                            return index;
                        }).second);
                    
                    auto& p = o.to<Layout>()->objects().at(i);
                    if(p) {
                        if(DynamicGUI::update_objects(gui, g, p, context, state)) {
                            //Print("Changed content");
                            dirty = true;
                        }
                    }
                }
            }
        }
        else if(auto vector = json_array_from_variable(loop_variable, props);
                vector.has_value())
        {
            std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
            ptrs.resize(vector->size());
            
            //State &state = *obj._state;
            auto scoped_handler = ensure_current_object_handler(state);
            //obj._state->_current_index = {};
            //obj._state->_collectors->objects.clear();
            
            size_t i = 0;
            auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});
            
            for(auto &v : *vector) {
                auto scope = scoped_handler->scope();
                scope.set(item_name, VarFunc(item_name, [v](const VarProps&) -> glz::json_t {
                    return v;
                }).second);
                scope.set("index", VarFunc("index", [i](const VarProps&) -> size_t {
                    return i;
                }).second);
                if(have_index)
                    scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                        return index;
                    }).second);
                
                // try to reuse existing objects first:
                Layout::Ptr ptr = ptrs.at(i);
                if(not ptr) {
                    //Print("Making new ", hash, " child.");
                    ptr = parse_object(gui, obj.child, context, state, context.defaults);
                } //else
                    //obj._state->_current_index.inc();

                if(ptr) {
                    if(DynamicGUI::update_objects(gui, g, ptr, context, state)) {
                        dirty = true;
                    }
                }

                ptrs.at(i) = std::move(ptr);

                ++i;
                if (i >= max_displayed_objects) {
                    break;
                }
            }
            
            ptrs.resize(i);
            o.to<Layout>()->set_children(std::move(ptrs));
            //obj.cache = vector;
        }
        else if(bool is_vector = loop_variable->is_vector();
                is_vector || loop_variable->is<sprite::Map>())
        {
            bool valid = true;
            if(not is_vector) {
                valid = loop_variable->access_value<sprite::Map>([&props](auto&& value){
                    if(props.subs.empty()
                       || not value.has(props.subs.front())
                       || not value.at(props.subs.front()).get().is_array())
                    {
                        return false;
                        //throw InvalidArgumentException("Cannot access ", props, " in ", value);
                    }
                    return true;
                }, props);
            }
            
            if(valid) {
                auto str = loop_variable->value_string(props);
                auto vector = util::parse_array_parts(util::truncate(str));
                
                //obj._state->_collectors->objects.clear();
                
                auto scoped_handler = ensure_current_object_handler(state);
                auto have_index = scoped_handler->evaluate_variable_value("index", VarProps{});
                
                std::vector<Layout::Ptr> ptrs = o.to<Layout>()->objects();
                if(ptrs.size() != min(max_displayed_objects, vector.size()))
                    dirty = true;
                ptrs.resize(vector.size());
                
                size_t i = 0;
                for(auto &v : vector) {
                    auto scope = scoped_handler->scope();
                    scope.set("index", VarFunc("index", [i](const VarProps&) -> size_t {
                        return i;
                    }).second);
                    scope.set(item_name, VarFunc(item_name, [v](const VarProps&) -> std::string {
                        return v;
                    }).second);
                    if(have_index)
                        scope.set("pindex", VarFunc("pindex", [index = Meta::fromStr<size_t>(*have_index)](const VarProps&) -> size_t {
                            return index;
                        }).second);
                    
                    // try to reuse existing objects first:
                    Layout::Ptr &ptr = ptrs.at(i);
                    if(not ptr) {
                        ptr = parse_object(gui, obj.child, context, state, context.defaults);
                        dirty = true;
                    }
                    if(ptr) {
                        if(DynamicGUI::update_objects(gui, g, ptr, context, state)) {
                            dirty = true;
                        } else if(ptr && ptr->is_dirty()) {
                            dirty = true;
                        }
                    }
                    
                    ++i;
                    if (i >= max_displayed_objects) {
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
            } else {
                error = true;
            }
        }
        else if(loop_variable->is<std::string>()) {
            auto str = loop_variable->value<std::string>(props);
            if(not generate_objects_from_string(str))
                error = true;
        }
        else {
            error = true;
        }
    }
    else {
        auto parsed = parse_text(obj.variable, context, state);
        if(utils::beginsWith(parsed, '[')
           && utils::endsWith(parsed, ']'))
        {
            if(not generate_objects_from_string(parsed))
                error = true;

            if(error) {
                auto ptrs = std::vector<Layout::Ptr>();
                auto text = settings::htmlify("Invalid loop for "+ parsed + " in "+glz::write_json(obj.child).value_or("null"));
                ptrs.push_back(Layout::Make<StaticText>{attr::Str{text}});
                o.to<Layout>()->set_children(std::move(ptrs));
                o.to<Layout>()->set_layout_dirty();
                o.to<Layout>()->update();
            }

            return dirty;
        }

        error = true;
    }
    
    if(error) {
        auto ptrs = std::vector<Layout::Ptr>();
        auto loop_text = typed_props.has_value()
            ? context.variable(typed_props->name, state)->value_string(*typed_props)
            : std::string(obj.variable);
        auto text = settings::htmlify("Invalid loop for "+ loop_text + " in "+glz::write_json(obj.child).value_or("null"));
        ptrs.push_back(Layout::Make<StaticText>{attr::Str{text}});
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
        if(ptr)
            register_delete_callback(it->second, ptr);
        return it->second;
    }
    
    return register_variant(hash, ptr);
}

const Pattern& PatternStore::set(size_t hash, const std::string& name, Pattern &&pattern) {
    auto it = _collectors->objects.find(hash);
    if(it == _collectors->objects.end()) {
        _collectors->objects[hash] = std::make_shared<HashedObject>();
    }
    
    auto& obj = _collectors->objects.at(hash)->patterns[name];
    obj = std::move(pattern);
    return obj;
}

std::optional<Pattern*> PatternStore::get(size_t hash, const std::string& name) {
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

std::optional<std::string_view> State::cached_variable_value(std::string_view name) const
{
    auto lock = _current_object_handler.lock();
    if(not lock)
        return std::nullopt;
    
    if(auto cached = lock->get_cached_variable_value(name);
       cached.has_value())
    {
        return cached;
    }
    
    // Loop-local/scoped variables (e.g. i, idx) are resolved from live values.
    if(lock->get_dynamic_variable(name) != nullptr)
    {
        return std::nullopt;
    }
    return lock->get_variable_value(name);
}

void State::set_cached_variable_value(std::string_view name, std::string_view value) {
    auto lock = _current_object_handler.lock();
    if(not lock)
        return;
    
    lock->set_cached_variable_value(name, value);
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

void Collectors::dealloc(size_t hash) {
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
