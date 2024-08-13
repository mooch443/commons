#include "ParseLayoutTypes.h"
#include <gui/LabeledField.h>
#include <gui/dyn/ParseText.h>
#include <gui/dyn/Action.h>

namespace cmn::gui::dyn {

Font parse_font(const glz::json_t::object_t& obj, Font font, std::string_view name) {
    if(auto it = obj.find(name);
       it != obj.end())
    {
        auto &f = it->second.get_object();
        if(f.count("size")) font.size = narrow_cast<Float2_t>(f.at("size").get<double>());
        if(f.count("style")) {
            auto style = f.at("style").get<std::string>();
            if(style == "bold")
                font.style = Style::Bold;
            if(style == "italic")
                font.style = Style::Italic;
            if(style == "regular")
                font.style = Style::Regular;
            if(style == "mono")
                font.style = Style::Monospace;
        }
        if(f.count("align")) {
            auto align = f.at("align").get<std::string>();
            if(align == "left")
                font.align = Align::Left;
            else if(align == "right")
                font.align = Align::Right;
            else if(align == "center")
                font.align = Align::Center;
        }
    }
    return font;
}

Image::Ptr load_image(const file::Path& path) {
    try {
        auto m = cv::imread(path.str(), cv::IMREAD_UNCHANGED);
        //auto ptr = ;
        //size_t hash = std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char*>(m.data), uint64_t(m.cols) * uint64_t(m.rows) * uint64_t(m.channels())));
        if(m.channels() != 1 && m.channels() != 4) {
            if(m.channels() == 3) {
                cv::cvtColor(m, m, cv::COLOR_RGB2RGBA);
            }
        }
        return Image::Make(m); //std::make_tuple(hash, std::move(ptr));
    } catch(...) {
        return nullptr;
    }
}

// Initialize from a JSON object
LayoutContext::LayoutContext(GUITaskQueue_t* gui, const glz::json_t::object_t& obj, State& state, const Context& context, DefaultSettings defaults, uint64_t hash)
 : gui(gui), obj(obj), state(state), context(context), _defaults(defaults)
{
    if(not obj.contains("type")) {
        //throw std::invalid_argument("Structure does not contain type information");
        type = LayoutType::unknown;
    } else {
        auto type_name = utils::lowercase(obj.at("type").get<std::string>());
        if (not LayoutType::has(type_name))
            type = LayoutType::unknown;
        else
            type = LayoutType::get(type_name);
    }
    
    if(hash == 0) {
        hash = state._current_index.hash();
        state._current_index.inc();
    }
    this->hash = hash;
    
    if(is_in(type, LayoutType::vlayout, LayoutType::hlayout, LayoutType::collection, LayoutType::gridlayout))
    {
        if(obj.contains("size")) {
            size = get(_defaults.size, "size");
        } else {
            //Print("Adding auto at ", hash, " for ", name, ": ", obj.dump(2));
            state.register_pattern(hash, "size", Pattern{"auto", {}});
        }
        
    } else {
        size = get(_defaults.size, "size");
    }
    
    scale = get(_defaults.scale, "scale");
    pos = get(_defaults.pos, "pos");
    origin = get(_defaults.origin, "origin");
    pad = get(_defaults.pad, "pad");
    name = get(std::string(), "name");
    fill = get(_defaults.fill, "fill");
    line = get(_defaults.line, "line");
    clickable = get(_defaults.clickable, "clickable");
    max_size = get(_defaults.max_size, "max_size");
    zindex = get(ZIndex(0), "z-index");
    
    textClr = get(_defaults.textClr, "color");
    highlight_clr = get(_defaults.highlightClr, "highlight_clr");
    vertical_clr = get(_defaults.verticalClr, "vertical_clr");
    horizontal_clr = get(_defaults.horizontalClr, "horizontal_clr");

    font = _defaults.font; // Initialize with default values
    if(type == LayoutType::button) {
        font.align = Align::Center;
    }
    font = parse_font(obj, font);
    
    /*if(auto it = state._timers.find(hash); it == state._timers.end()) {
        state._timers[hash].reset();
    }*/
}

void LayoutContext::finalize(const Layout::Ptr& ptr) {
    //Print("Calculating hash for index ", hash, " ", (uint64_t)ptr.get());
    if(type != LayoutType::settings) {
        if(line != Transparent)
            LabeledField::delegate_to_proper_type(attr::LineClr{line}, ptr);
        if(fill != Transparent)
            LabeledField::delegate_to_proper_type(attr::FillClr{fill}, ptr);
    }
    LabeledField::delegate_to_proper_type(attr::Margins{pad}, ptr);
    LabeledField::delegate_to_proper_type(font, ptr);
    LabeledField::delegate_to_proper_type(textClr, ptr);
    ptr->set(zindex);
    
    if(clickable)
        ptr->set_clickable(clickable);
    
    if(auto pattern = state.get_pattern(hash, "name");
       pattern.has_value())
    {
        name = parse_text(pattern.value()->original, context, state);
    }
    
    if(not name.empty()) {
        ptr->set_name(name);
        assert(ptr.get_smart());
        
        auto lock = state._current_object_handler.lock();
        if(lock) {
#ifndef NDEBUG
            /*if(auto n = state.named_entity(name);
               n != nullptr && n != ptr)
            {
                FormatWarning("Duplicate entry for ", name, ": ", n.get(), " vs. ", ptr.get());
            }*/
#endif
            lock->register_named(name, std::weak_ptr(ptr.get_smart()));
        }
    }
    
    if(obj.count("modules")) {
        if(obj.at("modules").is_array()) {
            for(auto mod : obj.at("modules").get_array()) {
                Print("module ", mod.get<std::string>());
                auto m = Modules::exists(mod.get<std::string>());
                if(m)
                    m->_apply(hash, state, ptr);
            }
        }
    }
    
    if(scale != Vec2(1)) ptr->set_scale(scale);
    if(pos != Vec2(0)) ptr->set_pos(pos);
    if(size != Vec2(0)) ptr->set_size(size);
    if(origin != Vec2(0)) ptr->set_origin(origin);
    
    if(obj.count("hover") || obj.count("unhover")) {
        auto hover_action = obj.count("hover") ? PreAction::fromStr(obj.at("hover").get<std::string>()) : PreAction{};
        auto unhover_action = obj.count("unhover") ? PreAction::fromStr(obj.at("unhover").get<std::string>()) : PreAction{};
        
        static constexpr auto hover_name = "_hover_handle";
        auto hover_handle = (Drawable::callback_handle_t::element_type*)ptr->custom_data(hover_name);
        
        auto handle = ptr->add_event_handler_replace(EventType::HOVER, [hover_action, unhover_action, _ptr = ptr.get(), context = context, &state = state](Event e) {
            if(e.hover.hovered
               && not hover_action.name.empty())
            {
                try {
                    auto name = parse_text(hover_action.name, context, state);
                    if(auto it = context.actions.find(name);
                       it != context.actions.end())
                    {
                        State state;
                        it->second(hover_action.parse(context, state));
                        
                    } else if(auto ptr = state.named_entity(name);
                              ptr != nullptr)
                    {
                        apply_modifier_to_object(name, ptr, hover_action.parse(context, state));
                        
                    } else {
                        Print("Unknown Action: ", hover_action);
                    }
                    
                } catch(...) {
                    FormatExcept("error using action ", hover_action);
                }
            } else if(not e.hover.hovered
                      && not unhover_action.name.empty())
            {
                try {
                    auto name = parse_text(unhover_action.name, context, state);
                    if(auto it = context.actions.find(name);
                       it != context.actions.end())
                    {
                        State state;
                        it->second(unhover_action.parse(context, state));
                        
                    } else if(auto ptr = state.named_entity(name);
                              ptr != nullptr)
                    {
                        apply_modifier_to_object(name, ptr, unhover_action.parse(context, state));
                        
                    } else {
                        Print("Unknown Action: ", unhover_action);
                    }
                    
                } catch(...) {
                    FormatExcept("error using action ", unhover_action);
                }
            }
        }, hover_handle);
        
        ptr->add_custom_data(hover_name, (void*)handle.get());
    }
    
    if(obj.count("drag") || obj.count("click")) {
        auto action = PreAction::fromStr(obj.at(obj.count("drag") ? "drag" : "click").get<std::string>());
        
        if(obj.count("drag")) {
            static constexpr auto handle_name = "_drag_handle";
            auto handle = (Drawable::callback_handle_t::element_type*)ptr->custom_data(handle_name);
            
            auto h = ptr->add_event_handler_replace(EventType::HOVER, [action, _ptr = ptr.get(), context = context](Event event) {
                if(event.hover.hovered
                   && _ptr->pressed())
                {
                    try {
                        if(auto it = context.actions.find(action.name);
                           it != context.actions.end())
                        {
                            State state;
                            it->second(action.parse(context, state));
                        } else
                            Print("Unknown Action: ", action);
                        
                    } catch(...) {
                        FormatExcept("error using action ", action);
                    }
                }
            }, handle);
            
            ptr->add_custom_data(handle_name, (void*)h.get());
        }
        
        static constexpr auto handle_name = "_drag_handle";
        auto handle = (Drawable::callback_handle_t::element_type*)ptr->custom_data(handle_name);
        auto h = ptr->add_event_handler_replace(EventType::MBUTTON, [action, context = context](Event event) {
            if(event.mbutton.pressed) {
                try {
                    if(auto it = context.actions.find(action.name);
                       it != context.actions.end())
                    {
                        State state;
                        it->second(action.parse(context, state));
                    } else
                        Print("Unknown Action: ", action);
                    
                } catch(...) {
                    FormatExcept("error using action ", action);
                }
            }
        }, handle);
        ptr->add_custom_data(handle_name, (void*)h.get());
    }
    
    ptr->add_custom_data("object_index", (void*)hash);
    //Print("adding object_index to ",ptr->type()," with index ", hash, " for ", (uint64_t)ptr.get());
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::combobox>() {
    throw std::invalid_argument("Combobox not implemented.");
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::image>()
{
    Image::Ptr img;
    
    if(obj.count("path") && obj.at("path").is_string()) {
        std::string raw = obj.at("path").get<std::string>();
        if(utils::contains(raw, "{")) {
            state.register_pattern(hash, "path", Pattern{raw, {}});
            
        } else {
            auto path = file::Path(raw);
            if(path.exists() && not path.is_folder()) {
                auto modified = path.last_modified();
                
                if(state._image_cache.contains(path.str())) {
                    auto &entry = state._image_cache.at(path.str());
                    if(std::get<0>(entry) != modified) {
                        // have to reload since the file changed
                        img = load_image(path);
                        if(not img)
                            throw U_EXCEPTION("Cannot load image at ", path,".");
                        
                        // successfully loaded, refresh entry
                        std::get<0>(entry) = modified;
                        std::get<1>(entry) = Image::Make(*img);
                        
                    } else {
                        // no need to reload... already up to date
                        img = Image::Make(*std::get<1>(entry));
                    }
                    
                } else {
                    img = load_image(path);
                    if(not img)
                        throw U_EXCEPTION("Cannot load image at ", path,".");
                    
                    state._image_cache[path.str()] = {modified, Image::Make(*img)};
                }
                
            } else
                throw U_EXCEPTION("Image at ",path," does not exist.");
        }
    }
    
    if(img) {
        return Layout::Make<ExternalImage>(std::move(img));
    } else {
        return Layout::Make<ExternalImage>();
    }
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::vlayout>()
{
    std::vector<Layout::Ptr> children;
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj.at("children").get_array()) {
            auto ptr = parse_object(gui, child.get_object(), context, state, context.defaults);
            if(ptr) {
                children.push_back(ptr);
            }
        }
    }
    
    auto ptr = Layout::Make<VerticalLayout>(std::move(children));
    if(obj.count("align")) {
        if(obj.at("align").is_string()) {
            std::string align = obj.at("align").get<std::string>();
            if(align == "left") {
                ptr.to<VerticalLayout>()->set_policy(VerticalLayout::Policy::LEFT);
            } else if(align == "center") {
                ptr.to<VerticalLayout>()->set_policy(VerticalLayout::Policy::CENTER);
            } else if(align == "right") {
                ptr.to<VerticalLayout>()->set_policy(VerticalLayout::Policy::RIGHT);
            } else FormatWarning("Unknown alignment: ", align);
        }
    }
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::hlayout>()
{
    std::vector<Layout::Ptr> children;
    
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj.at("children").get_array()) {
            auto ptr = parse_object(gui, child.get_object(), context, state, context.defaults);
            if(ptr) {
                children.push_back(ptr);
            }
        }
    }
    
    auto ptr = Layout::Make<HorizontalLayout>(std::move(children));
    if(obj.count("align")) {
        if(obj.at("align").is_string()) {
            std::string align = obj.at("align").get<std::string>();
            if(align == "top") {
                ptr.to<HorizontalLayout>()->set_policy(HorizontalLayout::Policy::TOP);
            } else if(align == "center") {
                ptr.to<HorizontalLayout>()->set_policy(HorizontalLayout::Policy::CENTER);
            } else if(align == "bottom") {
                ptr.to<HorizontalLayout>()->set_policy(HorizontalLayout::Policy::BOTTOM);
            } else FormatWarning("Unknown alignment: ", align);
        }
    }
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::gridlayout>()
{
    std::vector<Layout::Ptr> rows;
    GridLayout::HPolicy halign{GridLayout::HPolicy::LEFT};
    GridLayout::VPolicy valign{GridLayout::VPolicy::TOP};
    {
        auto _valign = utils::lowercase(get(std::string("top"), "valign"));
        auto _halign = utils::lowercase(get(std::string("left"), "halign"));
        if(_valign == "bottom")
            valign = GridLayout::VPolicy::BOTTOM;
        if(_valign == "center")
            valign = GridLayout::VPolicy::CENTER;
        if(_halign == "right")
            halign = GridLayout::HPolicy::RIGHT;
        if(_halign == "center")
            halign = GridLayout::HPolicy::CENTER;
    }
    
                
    if(obj.count("children")) {
        for(auto &row : obj.at("children").get_array()) {
            if(row.is_array()) {
                std::vector<Layout::Ptr> cols;
                for(auto &cell : row.get_array()) {
                    if(cell.is_array() || cell.is_object()) {
                        std::vector<Layout::Ptr> objects;
                        IndexScopeHandler handler{state._current_index};
                        if(cell.is_object()) {
                            // only a single object
                            auto ptr = parse_object(gui, cell.get_object(), context, state, context.defaults);
                            if(ptr)
                                objects.push_back(ptr);
                            
                        } else {
                            for(auto& obj : cell.get_array()) {
                                auto ptr = parse_object(gui, obj.get_object(), context, state, context.defaults);
                                if(ptr) {
                                    objects.push_back(ptr);
                                }
                            }
                        }
                        
                        cols.emplace_back(Layout::Make<Layout>(std::move(objects), attr::Margins{0,0,0,0}));
                        cols.back()->set_name("Col");
                        cols.back().to<Layout>()->update();
                        cols.back().to<Layout>()->update_layout();
                        cols.back().to<Layout>()->auto_size();
                    }
                }
                rows.emplace_back(Layout::Make<Layout>(std::move(cols), attr::Margins{0, 0, 0, 0}));
                rows.back()->set_name("Row");
                rows.back().to<Layout>()->update();
                rows.back().to<Layout>()->update_layout();
                rows.back().to<Layout>()->auto_size();
            }
        }
    }
    
    return Layout::Make<GridLayout>(valign, halign, std::move(rows), VerticalClr{vertical_clr}, HorizontalClr{horizontal_clr});
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::collection>()
{
    std::vector<Layout::Ptr> children;
    
    if(obj.count("children")) {
        //IndexScopeHandler handler{state._current_index};
        for(auto &child : obj.at("children").get_array()) {
            //Print("collection: ", child.dump());
            auto ptr = parse_object(gui, child.get_object(), context, state, context.defaults);
            if(ptr) {
                children.push_back(ptr);
            }
        }
    }
    
    auto ptr = Layout::Make<Layout>(std::move(children));
    ptr->set_name("collection");
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::settings>()
{
    Layout::Ptr ptr;
    std::string var;
    if(obj.contains("var")) {
        auto input = obj.at("var").get<std::string>();
        var = parse_text(input, context, state);
        if(var != input) {
            state.register_pattern(hash, "var", Pattern{input, {}});
        }
    } else
        throw U_EXCEPTION("settings field should contain a 'var'.");
    
    bool invert{false};
    if(obj.contains("invert") && obj.at("invert").is_boolean()) {
        invert = obj.at("invert").get<bool>();
    }
    
    {
        ptr = Layout::Make<HorizontalLayout>(Loc(), Box{0, 0, 0, 0}, Margins{0,0,0,0});
        
        auto hashed = state.get_monostate(hash, ptr);
        
        if(not hashed->_text_field) {
            hashed->_text_field = LabeledField::Make(gui, var, state, *this, invert);
            if(not hashed->_text_field) {
                FormatWarning("Cannot create representative of field ", var, " when creating controls for type ",settings_map()[var].get().type_name(),".");
                return nullptr;
            }
        }
        
        hashed->_var_cache = VarCache{
            ._var = var,
            ._obj = obj
        };
        
        auto& ref = hashed->_text_field;
        
        if(var.empty()) {
            // combobox?
            auto combo = static_cast<LabeledCombobox*>(ref.get());
            if(combo) {
                auto parm = combo->_combo->parameter();
                ref->replace_ref(parm);
            }
        }
        
        auto r = ref->ref();
        hashed->_var_cache->_value = r.valid()
                                     ? r.get().valueString()
                                     : "null";
        if(obj.contains("desc")) {
            ref->set_description(obj.at("desc").get<std::string>());
        }
        
        ref->set(font);
        ref->set(attr::FillClr{fill});
        ref->set(attr::LineClr{line});
        
        Placeholder_t placeholder{ get(std::string("Please select..."), "placeholder") };
        Postfix postfix { get(std::string(), "postfix") };
        ref->set(postfix);
        if(scale != Vec2(1)) ref->set(attr::Scale{scale});
        if(pos != Vec2(0)) ref->set(attr::Loc{pos});
        if(size != Vec2(0)) ref->set(attr::Size{size});
        
        ref->set(placeholder);
        
        if(origin != Vec2(0)) ref->set(attr::Origin{origin});
        if(max_size != Vec2(0)) ref->set(attr::SizeLimit{max_size});
        ref->set(attr::TextClr{textClr});
        ref->set(attr::HighlightClr{highlight_clr});
        
        if (obj.contains("label")) {
            auto &o = obj.at("label");
            if (o.is_object()) {
                auto &p = o.get_object();
                LabelBorderColor_t line_clr{ dyn::get(state, p, Color(200,200,200,200), "line", hash, "label_")};
                ref->set(line_clr);
                LabelColor_t fill_clr{ dyn::get(state, p, Color(50,50,50,200), "fill", hash, "label_")};
                ref->set(fill_clr);
                
                LabelFont_t label_font{parse_font(p, Font(0.75), "font")};
                ref->set(label_font);
            }
        }
        
        if (obj.contains("list")) {
            ref->set(LabelColor_t{fill});
            ref->set(LabelBorderColor_t{line});
            
            auto &o = obj.at("list");
            if (o.is_object()) {
                auto &p = o.get_object();
                
                ListLineClr_t line_clr{ dyn::get(state, p, Color(200,200,200,200), "line", hash, "list_")};
                ref->set(line_clr);
                ListFillClr_t fill_clr{ dyn::get(state, p, Color(50,50,50,200), "fill", hash, "list_")};
                ref->set(fill_clr);
                ListDims_t list_dims{ dyn::get(state, p, Size2(100,200), "size", hash, "list_") };
                ref->set(list_dims);
                ItemFont_t item_font{parse_font(p, Font(0.6), "font")};
                ref->set(item_font);
            }
        }
        
        std::vector<Layout::Ptr> objs;
        ref->add_to(objs);
        ptr.to<HorizontalLayout>()->set(std::move(objs));
        
        if(auto handler = state._current_object_handler.lock();
           handler)
        {
            handler->register_tooltipable(ref, ptr.get_smart());
            ref->register_delete_callback([_handler = state._current_object_handler, ptr = std::weak_ptr(ref)]()
            {
                auto handler = _handler.lock();
                if(not handler)
                    return;

                auto lock = ptr.lock();
                if(not lock)
                    return;

                handler->unregister_tooltipable(lock);
            });
        }
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::button>()
{
    std::string text = get(std::string(), "text");
    auto ptr = Layout::Make<Button>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    if(obj.count("action")) {
        auto action = PreAction::fromStr(obj.at("action").get<std::string>());
        ptr->on_click([action, context = context](auto){
            if(auto it = context.actions.find(action.name);
               it != context.actions.end())
            {
                State state;
                it->second(action.parse(context, state));
            } else
                Print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::checkbox>()
{
    std::string text = get(std::string(), "text");
    bool checked = get(false, "checked");
    
    auto ptr = Layout::Make<Checkbox>(attr::Str(text), attr::Checked(checked), font);
    
    if(obj.count("action")) {
        auto action = PreAction::fromStr(obj.at("action").get<std::string>());
        ptr.to<Checkbox>()->on_change([action, context=context](){
            if(auto it = context.actions.find(action.name);
               it != context.actions.end()) 
            {
                State state;
                it->second(action.parse(context, state));
            } else
                Print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::textfield>()
{
    std::string text = get(std::string(), "text");
    auto ptr = Layout::Make<Textfield>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    if(obj.count("action")) {
        auto action = PreAction::fromStr(obj.at("action").get<std::string>());
        ptr.to<Textfield>()->on_enter([action, context=context](){
            if(auto it = context.actions.find(action.name);
               it != context.actions.end()) 
            {
                State state;
                it->second(action.parse(context, state));
            } else
                Print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::stext>()
{
    std::string text = get(std::string(), "text");
    StaticText::FadeOut_t fade_out{get(0.f, "fade_out")};
    StaticText::Shadow_t shadow{get(0.f, "shadow")};
    Alpha alpha{get(1.f, "alpha")};
    
    return Layout::Make<StaticText>(
         Str{text},
         attr::Scale(scale),
         attr::Loc(pos),
         attr::Origin(origin),
         font,
         fade_out,
         shadow,
         attr::Margins(pad),
         attr::Name{text},
         attr::SizeLimit{max_size},
         alpha
    );
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::rect>() {
    auto ptr = Layout::Make<Rect>(attr::Scale(scale), attr::Loc(pos), attr::Size(size), attr::Origin(origin), FillClr{fill}, LineClr{line});
    
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::text>()
{
    Text::Shadow_t shadow{get(0.f, "shadow")};
    auto text = get(std::string(), "text");
    return Layout::Make<Text>(Str{text}, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font, shadow, Name{text});
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::circle>()
{
    Layout::Ptr ptr;
    Radius radius {get(5.f, "radius")};
    ptr = Layout::Make<Circle>(attr::Scale(scale), attr::Loc(pos), radius, attr::Origin(origin), attr::FillClr{fill}, attr::LineClr{line});

    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::each>()
{
    Layout::Ptr ptr;
    if(obj.count("do")) {
        auto &child = obj.at("do");
        if(obj.count("var") && obj.at("var").is_string() && child.is_object()) {
            //Print("collection: ", child.dump());
            // all successfull, add collection:
            
            ptr = Layout::Make<PlaceinLayout>(std::vector<Layout::Ptr>{});
            ptr->set_name("foreach<"+obj.at("var").get<std::string>()+" "+Meta::toStr(hash)+">");
            
            state.register_variant(hash, ptr, LoopBody{
                .variable = obj.at("var").get<std::string>(),
                .child = child.get_object()
            });
        }
    }
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::list>()
{
    Layout::Ptr ptr;
    if(obj.count("var") && obj.at("var").is_string() && obj.count("template")) {
        auto &child = obj.at("template");
        if(child.is_object()) {
            //Print("collection: ", child.dump());
            // all successfull, add collection:
            auto copy = std::make_unique<State>();
            copy->_current_object_handler = state._current_object_handler;
            
            ptr = Layout::Make<ScrollableList<DetailItem>>(Box{pos, size});
            auto body = state.register_variant(hash, ptr, ListContents{
                .variable = obj.at("var").get<std::string>(),
                .item = child.get_object(),
                ._state = std::move(copy)
            });
            
            ptr.to<ScrollableList<DetailItem>>()->on_select([&, body = std::weak_ptr(body)](size_t index, const DetailItem &)
            {
                auto lock = body.lock();
                if(not lock)
                    return;
                
                lock->apply_if<ListContents>([index](ListContents& contents){
                    if(contents.on_select_actions.contains(index)) {
                        std::get<1>(contents.on_select_actions.at(index))();
                    }
                });
            });
            ptr->set_name("list<"+obj.at("var").get<std::string>()+" "+Meta::toStr(hash)+">");
            
        }
        
    } else if(obj.count("items") && obj.at("items").is_array()) {
        auto& child = obj.at("items").get_array();
        
        ptr = Layout::Make<ScrollableList<DetailItem>>(Box{pos, size});
        std::vector<PreAction> actions;
        std::vector<DetailItem> items;
        
        for(auto &item : child) {
            if(item.is_object()) {
                try {
                    auto text = item.contains("text") && item["text"].is_string() ? item["text"].get<std::string>() : "";
                    auto detail = item.contains("detail") && item["detail"].is_string() ? item["detail"].get<std::string>() : "";
                    auto action = item.contains("action") && item["action"].is_string() ? item["action"].get<std::string>() : "";
                    auto disabled = item.contains("disabled") && item["disabled"].is_boolean() ? item["disabled"].get<bool>() : false;
                    if(item.contains("disabled") && item["disabled"].is_string()) {
                        disabled = Meta::fromStr<bool>(parse_text(item["disabled"].get<std::string>(), context, state));
                    }
                    
                    actions.push_back(PreAction::fromStr(action));
                    //Print("list item: ", text, " ", action);
                    items.push_back(DetailItem{
                        text,
                        detail,
                        disabled
                    });
                    
                } catch(const std::exception& ex) {
                    actions.push_back(PreAction());
                    items.push_back(DetailItem{
                        "Error",
                        ex.what(),
                        true
                    });
                }
            }
        }
        
        state.register_variant(hash, ptr, ManualListContents{
            .items = items
        });
        
        ptr.to<ScrollableList<DetailItem>>()->set_items(items);
        ptr.to<ScrollableList<DetailItem>>()->on_select([actions, context=context](size_t index, const DetailItem &)
        {
            if(index >= actions.size()) {
                FormatWarning("Cannot select invalid index: ", index, " from ", actions);
                return;
            }
            
            auto& action = actions.at(index);
            if(auto it = context.actions.find(action.name);
               it != context.actions.end())
            {
                State state;
                //Print("Selecting ", actions.at(index));
                it->second(action.parse(context, state));
            }
        });
    }
    
    if(ptr) {
        auto list = ptr.to<ScrollableList<DetailItem>>();
        
        ItemPadding_t item_padding{get(Vec2(5), "item_padding")};
        list->set(item_padding);
        
        Placeholder_t placeholder{get(std::string(), "placeholder")};
        list->set(placeholder);
        
        ItemFont_t item_font{parse_font(obj, Font(0.75), "item_font")};
        list->set(item_font);
        DetailFont_t detail_font{parse_font(obj, Font(0.55, Align::Center), "detail_font")};
        list->set(detail_font);

        ItemColor_t item_color{get(Color(50,50,50,220), "item_color")};
        list->set(item_color);
        ItemBorderColor_t item_line{get(Transparent, "item_line")};
        list->set(item_line);
        
        Alternating_t alternate{get(false, "alternate")};
        list->set(alternate);

        DetailColor_t detail_color{get(Gray, "detail_color")};
        list->set(detail_color);
        
        if (obj.contains("label")) {
            auto &o = obj.at("label");
            if (o.is_object()) {
                auto &p = o.get_object();
                
                LabelBorderColor_t line_clr{ dyn::get(state, p, Color(200,200,200,200), "line", hash, "label_")};
                list->set(line_clr);
                LabelColor_t fill_clr{ dyn::get(state, p, Color(50,50,50,200), "fill", hash, "label_")};
                list->set(fill_clr);
                
                LabelFont_t label_font{parse_font(p, Font(0.75), "font")};
                list->set(label_font);
            }
        }

        if (obj.contains("list")) {
            auto &o = obj.at("list");
            if (o.is_object()) {
                auto &p = o.get_object();
                ListLineClr_t line_clr{ dyn::get(state, p, Color(200,200,200,200), "line", hash, "list_")};
                list->set(line_clr);
                ListFillClr_t fill_clr{ dyn::get(state, p, Color(50,50,50,200), "fill", hash, "list_")};
                list->set(fill_clr);
                ListDims_t list_dims{ dyn::get(state, p, Size2(100,200), "size", hash, "list_") };
                list->set(list_dims);
            }
        }
        
        Foldable_t foldable{get(false, "foldable")};
        Folded_t folded{get(false, "folded")};
        Str folded_label{get(std::string(), "text")};
        
        if(foldable) {
            list->set(foldable);
            list->set(folded);
        }
        
        if(not folded_label.empty())
            list->set(folded_label);
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::condition>()
{
    Layout::Ptr ptr;
    if(obj.count("then")) {
        auto &_child = obj.at("then");
        if(obj.count("var") && obj.at("var").is_string() && _child.is_object()) {
            auto &child = _child.get_object();
            auto copy = std::make_unique<State>();
            copy->_current_object_handler = state._current_object_handler;
            
            // inc another two times to reserve our object
            //state._current_index.inc();
            //state._current_index.inc();
            
            ptr = Layout::Make<PlaceinLayout>(std::vector<Layout::Ptr>{});
            //ptr->clear_delete_handlers();
            auto weak = std::weak_ptr(state.register_variant(hash, ptr, IfBody{
                .variable = obj.at("var").get<std::string>(),
                .__if = child,
                .__else = obj.contains("else") ? obj.at("else") : nullptr,
                ._assigned_hash = hash,
                ._state = std::move(copy)
            }));
        }
    }
    return ptr;
}

}
