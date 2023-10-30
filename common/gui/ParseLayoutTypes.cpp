#include "ParseLayoutTypes.h"

namespace gui::dyn {

Font parse_font(const nlohmann::json& obj, Font font) {
    if(obj.count("font")) {
        auto f = obj["font"];
        if(f.count("size")) font.size = f["size"].get<float>();
        if(f.count("style")) {
            auto style = f["style"].get<std::string>();
            if(style == "bold")
                font.style = Style::Bold;
            if(style == "italic")
                font.style = Style::Italic;
            if(style == "regular")
                font.style = Style::Regular;
        }
        if(f.count("align")) {
            auto align = f["align"].get<std::string>();
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
LayoutContext::LayoutContext(const nlohmann::json& obj, State& state, DefaultSettings defaults, uint64_t hash)
 : obj(obj), state(state), _defaults(defaults)
{
    if(not obj.contains("type"))
        throw std::invalid_argument("Structure does not contain type information");
    
    auto type_name = utils::lowercase(obj["type"].get<std::string>());
    type = LayoutType::get(type_name);
    
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
            //print("Adding auto at ", hash, " for ", name, ": ", obj.dump(2));
            state.patterns[hash]["size"] = "auto";
        }
        
    } else {
        size = get(_defaults.size, "size");
    }
    
    scale = get(_defaults.scale, "scale");
    pos = get(_defaults.pos, "pos");
    origin = get(_defaults.origin, "origin");
    pad = get(_defaults.pad, "pad");
    name = get(_defaults.name, "name");
    fill = get(_defaults.fill, "fill");
    line = get(_defaults.line, "line");
    clickable = get(_defaults.clickable, "clickable");
    max_size = get(_defaults.max_size, "max_size");
    
    textClr = get(_defaults.textClr, "color");
    highlight_clr = get(_defaults.highlightClr, "highlight_clr");
    vertical_clr = get(_defaults.verticalClr, "vertical_clr");
    horizontal_clr = get(_defaults.horizontalClr, "horizontal_clr");

    font = _defaults.font; // Initialize with default values
    if(type == LayoutType::button) {
        font.align = Align::Center;
    }
    font = parse_font(obj, font);
}

void LayoutContext::finalize(const Layout::Ptr& ptr) {
    //print("Calculating hash for index ", hash, " ", (uint64_t)ptr.get());
    if(type != LayoutType::settings) {
        if(line != Transparent)
            LabeledField::delegate_to_proper_type(attr::LineClr{line}, ptr);
        if(fill != Transparent)
            LabeledField::delegate_to_proper_type(attr::FillClr{fill}, ptr);
    }
    LabeledField::delegate_to_proper_type(attr::Margins{pad}, ptr);
    LabeledField::delegate_to_proper_type(font, ptr);
    LabeledField::delegate_to_proper_type(textClr, ptr);
    
    if(clickable)
        ptr->set_clickable(clickable);
    
    if(not name.empty())
        ptr->set_name(name);
    
    if(obj.count("modules")) {
        if(obj["modules"].is_array()) {
            for(auto mod : obj["modules"]) {
                print("module ", mod.get<std::string>());
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
    
    ptr->add_custom_data("object_index", (void*)hash);
    //print("adding object_index to ",ptr->type()," with index ", hash, " for ", (uint64_t)ptr.get());
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::combobox>(const Context&) {
    throw std::invalid_argument("Combobox not implemented.");
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::image>(const Context&)
{
    Image::Ptr img;
    
    if(obj.count("path") && obj["path"].is_string()) {
        std::string raw = obj["path"].get<std::string>();
        if(utils::contains(raw, "{")) {
            state.patterns[hash]["path"] = raw;
            
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
Layout::Ptr LayoutContext::create_object<LayoutType::vlayout>(const Context& context)
{
    std::vector<Layout::Ptr> children;
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj["children"]) {
            auto ptr = parse_object(child, context, state, context.defaults);
            if(ptr) {
                children.push_back(ptr);
            }
        }
    }
    
    auto ptr = Layout::Make<VerticalLayout>(std::move(children));
    if(obj.count("align")) {
        if(obj["align"].is_string()) {
            std::string align = obj["align"].get<std::string>();
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
Layout::Ptr LayoutContext::create_object<LayoutType::hlayout>(const Context& context)
{
    std::vector<Layout::Ptr> children;
    
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj["children"]) {
            auto ptr = parse_object(child, context, state, context.defaults);
            if(ptr) {
                children.push_back(ptr);
            }
        }
    }
    
    auto ptr = Layout::Make<HorizontalLayout>(std::move(children));
    if(obj.count("align")) {
        if(obj["align"].is_string()) {
            std::string align = obj["align"].get<std::string>();
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
Layout::Ptr LayoutContext::create_object<LayoutType::gridlayout>(const Context& context)
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
        for(auto &row : obj["children"]) {
            if(row.is_array()) {
                std::vector<Layout::Ptr> cols;
                for(auto &cell : row) {
                    if(cell.is_array() || cell.is_object()) {
                        std::vector<Layout::Ptr> objects;
                        IndexScopeHandler handler{state._current_index};
                        if(cell.is_object()) {
                            // only a single object
                            auto ptr = parse_object(cell, context, state, context.defaults);
                            if(ptr)
                                objects.push_back(ptr);
                            
                        } else {
                            for(auto& obj : cell) {
                                auto ptr = parse_object(obj, context, state, context.defaults);
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
Layout::Ptr LayoutContext::create_object<LayoutType::collection>(const Context& context)
{
    std::vector<Layout::Ptr> children;
    
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj["children"]) {
            //print("collection: ", child.dump());
            auto ptr = parse_object(child, context, state, context.defaults);
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
Layout::Ptr LayoutContext::create_object<LayoutType::settings>(const Context& context)
{
    Layout::Ptr ptr;
    std::string var;
    if(obj.contains("var")) {
        auto input = obj["var"].get<std::string>();
        var = parse_text(input, context);
        if(var != input) {
            state.patterns[hash]["var"] = input;
        }
    } else
        throw U_EXCEPTION("settings field should contain a 'var'.");
    
    bool invert{false};
    if(obj.contains("invert") && obj["invert"].is_boolean()) {
        invert = obj["invert"].get<bool>();
    }
    
    {
        {
            auto ptr = LabeledField::Make(var, obj, invert);
            if(not state._text_fields.contains(hash) or not state._text_fields.at(hash))
            {
                state._text_fields.emplace(hash, std::move(ptr));
            } else {
                //throw U_EXCEPTION("Cannot deal with replacement LabeledFields yet.");
                state._text_fields[hash] = std::move(ptr);
            }
        }
        
        VarCache& cache = state._var_cache[hash];
        cache._var = var;
        cache._obj = obj;
        
        auto& ref = state._text_fields.at(hash);
        if(not ref) {
            FormatWarning("Cannot create representative of field ", var, " when creating controls for type ",settings_map()[var].get().type_name(),".");
            return nullptr;
        }
        
        cache._value = ref->_ref.get().valueString();
        if(obj.contains("desc")) {
            ref->set_description(obj["desc"].get<std::string>());
        }
        
        ref->set(font);
        ref->set(attr::FillClr{fill});
        ref->set(attr::LineClr{line});
        if(scale != Vec2(1)) ref->set(attr::Scale{scale});
        if(pos != Vec2(0)) ref->set(attr::Loc{pos});
        if(size != Vec2(0)) ref->set(attr::Size{size});
        if(origin != Vec2(0)) ref->set(attr::Origin{origin});
        if(max_size != Vec2(0)) ref->set(attr::SizeLimit{max_size});
        ref->set(attr::TextClr{textClr});
        ref->set(attr::HighlightClr{highlight_clr});
        
        std::vector<Layout::Ptr> objs;
        ref->add_to(objs);
        ptr = Layout::Make<HorizontalLayout>(std::move(objs), Loc(), Box{0, 0, 0, 0}, Margins{0,0,0,0});
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::button>(const Context& context)
{
    std::string text = get(std::string(), "text");
    auto ptr = Layout::Make<Button>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    Action action;
    if(obj.count("action")) {
        action = Action::fromStr(obj["action"].get<std::string>());
        ptr->on_click([action, context](auto){
            if(context.actions.contains(action.name)) {
                auto copy = action;
                for(auto &p : copy.parameters)
                    p = parse_text(p, context);
                context.actions.at(action.name)(copy);
            } else
                print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::checkbox>(const Context& context)
{
    std::string text = get(std::string(), "text");
    bool checked = get(false, "checked");
    
    auto ptr = Layout::Make<Checkbox>(attr::Str(text), attr::Checked(checked), font);
    
    if(obj.count("action")) {
        auto action = Action::fromStr(obj["action"].get<std::string>());
        ptr.to<Checkbox>()->on_change([action, context](){
            if(context.actions.contains(action.name))
                context.actions.at(action.name)(action);
            else
                print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::textfield>(const Context& context)
{
    std::string text = get(std::string(), "text");
    auto ptr = Layout::Make<Textfield>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    if(obj.count("action")) {
        auto action = Action::fromStr(obj["action"].get<std::string>());
        ptr.to<Textfield>()->on_enter([action, context](){
            if(context.actions.contains(action.name))
                context.actions.at(action.name)(action);
            else
                print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::stext>(const Context&)
{
    std::string text = get(std::string(), "text");
    StaticText::FadeOut_t fade_out{get(0.f, "fade_out")};
    StaticText::Shadow_t shadow{get(0.f, "shadow")};
    
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
         attr::SizeLimit{max_size}
    );
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::rect>(const Context& context) {
    auto ptr = Layout::Make<Rect>(attr::Scale(scale), attr::Loc(pos), attr::Size(size), attr::Origin(origin), FillClr{fill}, LineClr{line});
    
    if(obj.count("action")) {
        Action action = Action::fromStr(obj["action"].get<std::string>());
        auto context_copy = context;
        
        context_copy.variables.insert(VarFunc("mouse", [_ptr = ptr.get()](VarProps){
            return _ptr->parent() && _ptr->parent()->stage() ? _ptr->parent()->stage()->mouse_position() : Vec2();
        }));
        
        ptr->add_event_handler(EventType::HOVER, [action, _ptr = ptr.get(), context = std::move(context_copy)](Event event) {
            if(event.hover.hovered
               && _ptr->pressed()) 
            {
                print("Dragging: ", event.hover.x);
                try {
                    if(context.actions.contains(action.name)) {
                        auto copy = action;
                        for(auto&p : copy.parameters) {
                            auto c = p;
                            p = parse_text(p, context);
                            print(c," => ", p);
                        }
                        
                        context.actions.at(action.name)(copy);
                    } else
                        print("Unknown Action: ", action);
                    
                } catch(...) {
                    FormatExcept("error using action ", action);
                }
            }
        });
        
        /*ptr->on_click([action, context = std::move(context_copy)](auto){
            try {
                if(context.actions.contains(action.name)) {
                    auto copy = action;
                    for(auto&p : copy.parameters) {
                        auto c = p;
                        p = parse_text(p, context);
                        print(c," => ", p);
                    }
                    
                    context.actions.at(action.name)(copy);
                } else
                    print("Unknown Action: ", action);
                
            } catch(...) {
                FormatExcept("error using action ", action);
            }
        });*/
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::text>(const Context&)
{
    Text::Shadow_t shadow{get(0.f, "shadow")};
    auto text = get(std::string(), "text");
    return Layout::Make<Text>(Str{text}, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font, shadow, Name{text});
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::circle>(const Context&)
{
    Layout::Ptr ptr;
    Radius radius {get(5.f, "radius")};
    ptr = Layout::Make<Circle>(attr::Scale(scale), attr::Loc(pos), radius, attr::Origin(origin), attr::FillClr{fill}, attr::LineClr{line});

    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::each>(const Context&)
{
    Layout::Ptr ptr;
    if(obj.count("do")) {
        auto child = obj["do"];
        if(obj.count("var") && obj["var"].is_string() && child.is_object()) {
            //print("collection: ", child.dump());
            // all successfull, add collection:
            state.loops[hash] = {
                .variable = obj["var"].get<std::string>(),
                .child = child,
                .state = std::make_unique<State>(state)
            };
            ptr = Layout::Make<PlaceinLayout>(std::vector<Layout::Ptr>{});
            ptr->set_name("foreach<"+obj["var"].get<std::string>()+" "+Meta::toStr(hash)+">");
        }
    }
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::list>(const Context& context)
{
    Layout::Ptr ptr;
    if(obj.count("var") && obj["var"].is_string() && obj.count("template")) {
        auto child = obj["template"];
        if(child.is_object()) {
            //print("collection: ", child.dump());
            // all successfull, add collection:
            state.lists[hash] = {
                .variable = obj["var"].get<std::string>(),
                .item = child,
                .state = std::make_unique<State>(state)
            };
            
            ptr = Layout::Make<ScrollableList<DetailItem>>(Box{pos, size});
            ptr.to<ScrollableList<DetailItem>>()->on_select([&, &body = state.lists[hash]](size_t index, const DetailItem &)
            {
                if(body.on_select_actions.contains(index)) {
                    std::get<1>(body.on_select_actions.at(index))();
                }
            });
            ptr->set_name("list<"+obj["var"].get<std::string>()+" "+Meta::toStr(hash)+">");
            
        }
        
    } else if(obj.count("items") && obj["items"].is_array()) {
        auto child = obj["items"];
        
        ptr = Layout::Make<ScrollableList<DetailItem>>(Box{pos, size});
        std::vector<Action> actions;
        std::vector<DetailItem> items;
        
        for(auto &item : child) {
            if(item.is_object()) {
                auto text = item.contains("text") && item["text"].is_string() ? item["text"].get<std::string>() : "";
                auto detail = item.contains("detail") && item["detail"].is_string() ? item["detail"].get<std::string>() : "";
                auto action = item.contains("action") && item["action"].is_string() ? item["action"].get<std::string>() : "";
                
                actions.push_back(Action::fromStr(action));
                print("list item: ", text, " ", action);
                items.push_back(DetailItem{
                    text,
                    detail
                });
            }
        }
        
        ptr.to<ScrollableList<DetailItem>>()->set_items(items);
        ptr.to<ScrollableList<DetailItem>>()->on_select([actions, context](size_t index, const DetailItem &)
        {
            if(index >= actions.size()) {
                FormatWarning("Cannot select invalid index: ", index, " from ", actions);
                return;
            }
            
            print("Selecting ", actions.at(index));
            auto copy = actions.at(index);
            for(auto &p : copy.parameters) {
                p = parse_text(p, context);
            }
            print(" => ", copy);
            
            if(context.actions.contains(copy.name)) {
                context.actions.at(copy.name)(copy);
            }
        });
    }
    
    if(ptr) {
        ItemHeight_t item_height{get(float(5), "item_height")};
        auto list = ptr.to<ScrollableList<DetailItem>>();
        list->set(item_height);
        
        Alternating_t alternate{get(false, "alternate")};
        list->set(alternate);
        
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
Layout::Ptr LayoutContext::create_object<LayoutType::condition>(const Context& context)
{
    Layout::Ptr ptr;
    if(obj.count("then")) {
        auto child = obj["then"];
        if(obj.count("var") && obj["var"].is_string() && child.is_object()) {
            ptr = Layout::Make<PlaceinLayout>(std::vector<Layout::Ptr>{});
            IndexScopeHandler handler{state._current_index};
            auto c = parse_object(child, context, state, context.defaults);
            if(not c)
                return nullptr;
            
            c->set_is_displayed(false);
            
            Layout::Ptr _else;
            if(obj.count("else") && obj["else"].is_object()) {
                _else = parse_object(obj["else"], context, state, context.defaults);
            }
            
            state.ifs[hash] = IfBody{
                .variable = obj["var"].get<std::string>(),
                ._if = c,
                ._else = _else
            };
        }
    }
    return ptr;
}

}
