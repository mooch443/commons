#include "ParseLayoutTypes.h"

namespace gui::dyn {

Image::Ptr load_image(const file::Path& path) {
    try {
        auto m = cv::imread(path.str());
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
LayoutContext::LayoutContext(const nlohmann::json& obj, State& state)
 : obj(obj), state(state)
{
    auto type_name = utils::lowercase(obj["type"].get<std::string>());
    type = LayoutType::get(type_name);
    
    hash = state._current_index.hash();
    state._current_index.inc();
    
    if(is_in(type, LayoutType::vlayout, LayoutType::hlayout, LayoutType::collection, LayoutType::gridlayout))
    {
        if(obj.contains("size")) {
            size = get(Size2(0), "size");
        } else {
            //print("Adding auto at ", hash, " for ", name, ": ", obj.dump(2));
            state.patterns[hash]["size"] = "auto";
        }
        
    } else {
        size = get(Size2(0), "size");
    }
    
    scale = get(Vec2(1), "scale");
    pos = get(Vec2(0), "pos");
    origin = get(Vec2(0), "origin");
    margins = get(Bounds(5,5,5,5), "margins");
    name = get(std::string(), "name");
    fill = get(Transparent, "fill");
    line = get(Transparent, "line");
    clickable = get(false, "clickable");

    font = Font(0.75); // Initialize with default values
    if(type == LayoutType::button) {
        font.align = Align::Center;
    }
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
}

void LayoutContext::finalize(const Layout::Ptr& ptr) {
    //print("Calculating hash for index ", hash, " ", (uint64_t)ptr.get());
    
    if(type != LayoutType::settings && ptr.is<SectionInterface>()) {
        if(fill != Transparent) {
            if(line != Transparent)
                ptr.to<SectionInterface>()->set_background(fill, line);
            else
                ptr.to<SectionInterface>()->set_background(fill);
        } else if(line != Transparent)
            ptr.to<SectionInterface>()->set_background(Transparent, line);
    }
    
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
            auto ptr = parse_object(child, context, state);
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
            auto ptr = parse_object(child, context, state);
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
                
    if(obj.count("children")) {
        for(auto &row : obj["children"]) {
            if(row.is_array()) {
                std::vector<Layout::Ptr> cols;
                for(auto &cell : row) {
                    if(cell.is_array()) {
                        std::vector<Layout::Ptr> objects;
                        IndexScopeHandler handler{state._current_index};
                        for(auto& obj : cell) {
                            auto ptr = parse_object(obj, context, state);
                            if(ptr) {
                                objects.push_back(ptr);
                            }
                        }
                        cols.emplace_back(Layout::Make<Layout>(std::move(objects)));
                        cols.back()->set_name("Col");
                        cols.back().to<Layout>()->update();
                        cols.back().to<Layout>()->update_layout();
                        cols.back().to<Layout>()->auto_size({});
                    }
                }
                rows.emplace_back(Layout::Make<Layout>(std::move(cols)));
                rows.back()->set_name("Row");
                rows.back().to<Layout>()->set(LineClr{Yellow});
                rows.back().to<Layout>()->update();
                rows.back().to<Layout>()->update_layout();
                rows.back().to<Layout>()->auto_size({});
            }
        }
    }
    
    auto ptr = Layout::Make<GridLayout>();
    ptr.to<GridLayout>()->set_children(std::move(rows));
    
    auto vertical_clr = get(DarkCyan.alpha(150), "vertical_clr");
    auto horizontal_clr = get(DarkGray.alpha(150), "horizontal_clr");
    
    ptr.to<GridLayout>()->set(VerticalClr{vertical_clr});
    ptr.to<GridLayout>()->set(HorizontalClr{horizontal_clr});

    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::collection>(const Context& context)
{
    std::vector<Layout::Ptr> children;
    
    if(obj.count("children")) {
        IndexScopeHandler handler{state._current_index};
        for(auto &child : obj["children"]) {
            //print("collection: ", child.dump());
            auto ptr = parse_object(child, context, state);
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
Layout::Ptr LayoutContext::create_object<LayoutType::settings>(const Context&)
{
    Layout::Ptr ptr;
    std::string var;
    if(obj.contains("var")) {
        var = obj["var"].get<std::string>();
    } else
        throw U_EXCEPTION("settings field should contain a 'var'.");
    
    bool invert{false};
    if(obj.contains("invert") && obj["invert"].is_boolean()) {
        invert = obj["invert"].get<bool>();
    }
    
    {
        auto &ref = state._text_fields[var];
        ref = LabeledField::Make(var, invert);
        
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
        
        Color color{White};
        if(obj.count("color")) {
            if(obj["color"].is_string())
                state.patterns[hash]["color"] = obj["color"].get<std::string>();
            else {
                color = parse_color(obj["color"]);
                ref->set(attr::TextClr{color});
            }
        }
        
        std::vector<Layout::Ptr> objs;
        ref->add_to(objs);
        ptr = Layout::Make<HorizontalLayout>(std::move(objs), Vec2(), Bounds{0, 0, 0, 0});
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::button>(const Context& context)
{
    std::string text;
    if(obj.contains("text")) {
        text = obj["text"].get<std::string>();
    }
    
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    auto ptr = Layout::Make<Button>(text, attr::Scale(scale), attr::Origin(origin), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr->on_click([action, context](auto e){
            if(context.actions.contains(action))
                context.actions.at(action)(e);
            else
                print("Unknown Action: ", action);
        });
    }
    
    Color color{White};
    if(obj.count("color")) {
        if(obj["color"].is_string())
            state.patterns[hash]["color"] = obj["color"].get<std::string>();
        else {
            color = parse_color(obj["color"]);
            ptr.to<Button>()->set_text_clr(color);
        }
    }
    
    if(fill != Transparent)
        ptr.to<Button>()->set_fill_clr(fill);
    if(line != Transparent)
        ptr.to<Button>()->set_line_clr(line);
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::checkbox>(const Context& context)
{
    std::string text;
    if(obj.contains("text")) {
        text = obj["text"].get<std::string>();
    }
    
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    bool checked = false;
    if(obj.contains("checked")) {
        checked = obj["checked"].get<bool>();
    }
    
    auto ptr = Layout::Make<Checkbox>(attr::Loc(), text, attr::Checked(checked), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr.to<Checkbox>()->on_change([action, context](){
            if(context.actions.contains(action))
                context.actions.at(action)(Event(EventType::KEY));
            else
                print("Unknown Action: ", action);
        });
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::textfield>(const Context& context)
{
    std::string text;
    if(obj.contains("text")) {
        text = obj["text"].get<std::string>();
    }
    
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    auto ptr = Layout::Make<Textfield>(attr::Content(text), attr::Scale(scale), attr::Origin(origin), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr.to<Textfield>()->on_enter([action, context](){
            if(context.actions.contains(action))
                context.actions.at(action)(Event(EventType::KEY));
            else
                print("Unknown Action: ", action);
        });
    }
    
    Color color{White};
    if(obj.count("color")) {
        if(obj["color"].is_string())
            state.patterns[hash]["color"] = obj["color"].get<std::string>();
        else {
            color = parse_color(obj["color"]);
            ptr.to<Textfield>()->set_text_color(color);
        }
    }
    
    if(fill != Transparent)
        ptr.to<Textfield>()->set_fill_color(fill);
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::stext>(const Context&)
{
    Layout::Ptr ptr;
    std::string text;
    if(obj.contains("text")) {
        text = obj["text"].get<std::string>();
    }
    
    Size2 max_size = get(Size2(0), "max_size");
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    ptr = Layout::Make<StaticText>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
    
    //if(margins != Margins{0, 0, 0, 0})
    {
        ptr.to<StaticText>()->set(attr::Margins(margins));
        if(not max_size.empty())
            ptr.to<StaticText>()->set_max_size(max_size);
    }
    
    Color color{White};
    if(obj.count("color")) {
        if(obj["color"].is_string())
            state.patterns[hash]["color"] = obj["color"].get<std::string>();
        else {
            color = parse_color(obj["color"]);
            ptr.to<StaticText>()->set_text_color(color);
        }
    }
    
    ptr->set_name(text);
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::rect>(const Context&) {
    auto ptr = Layout::Make<Rect>(attr::Scale(scale), attr::Loc(pos), attr::Size(size), attr::Origin(origin));
    
    if(fill != Transparent)
        ptr.to<Rect>()->set_fillclr(fill);
    if(line != Transparent)
        ptr.to<Rect>()->set_lineclr(line);

    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::text>(const Context&)
{
    Layout::Ptr ptr;

    std::string text;
    if(obj.contains("text")) {
        text = obj["text"].get<std::string>();
    }
    
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    ptr = Layout::Make<Text>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
    ptr->set_name(text);
    
    Color color{White};
    if(obj.count("color")) {
        if(obj["color"].is_string())
            state.patterns[hash]["color"] = obj["color"].get<std::string>();
        else {
            color = parse_color(obj["color"]);
            ptr.to<Text>()->set_color(color);
        }
    }
    
    return ptr;
}

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::circle>(const Context&)
{
    Layout::Ptr ptr;
    float radius = 0;
    if(obj.count("radius")) {
        if(obj["radius"].is_number())
            radius = obj["radius"].get<float>();
    }
    ptr = Layout::Make<Circle>(attr::Scale(scale), attr::Loc(pos), attr::Radius(radius), attr::Origin(origin), attr::FillClr{fill}, attr::LineClr{line});

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
            ptr = Layout::Make<Layout>(std::vector<Layout::Ptr>{});
            ptr->set_name("foreach<"+obj["var"].get<std::string>()+" "+Meta::toStr(hash)+">");
        }
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
            ptr = Layout::Make<Layout>(std::vector<Layout::Ptr>{});
            IndexScopeHandler handler{state._current_index};
            auto c = parse_object(child, context, state);
            c->set_is_displayed(false);
            
            Layout::Ptr _else;
            if(obj.count("else") && obj["else"].is_object()) {
                _else = parse_object(obj["else"], context, state);
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
