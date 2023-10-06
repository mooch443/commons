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
LayoutContext::LayoutContext(const nlohmann::json& obj, State& state, DefaultSettings defaults)
 : obj(obj), state(state), _defaults(defaults)
{
    if(not obj.contains("type"))
        throw std::invalid_argument("Structure does not contain type information");
    
    auto type_name = utils::lowercase(obj["type"].get<std::string>());
    type = LayoutType::get(type_name);
    
    hash = state._current_index.hash();
    state._current_index.inc();
    
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
    margins = get(_defaults.margins, "margins");
    name = get(_defaults.name, "name");
    fill = get(_defaults.fill, "fill");
    line = get(_defaults.line, "line");
    clickable = get(_defaults.clickable, "clickable");
    max_size = get(_defaults.max_size, "max_size");
    
    textClr = _defaults.textClr;
    if(obj.count("color")) {
        if(obj["color"].is_string())
            state.patterns[hash]["color"] = obj["color"].get<std::string>();
        else
            textClr = parse_color(obj["color"]);
    }

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
    LabeledField::delegate_to_proper_type(attr::Margins{margins}, ptr);
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
                    if(cell.is_array()) {
                        std::vector<Layout::Ptr> objects;
                        IndexScopeHandler handler{state._current_index};
                        for(auto& obj : cell) {
                            auto ptr = parse_object(obj, context, state, context.defaults);
                            if(ptr) {
                                objects.push_back(ptr);
                            }
                        }
                        cols.emplace_back(Layout::Make<Layout>(std::move(objects), attr::Margins{0,0,0,0}));
                        cols.back()->set_name("Col");
                        cols.back().to<Layout>()->update();
                        cols.back().to<Layout>()->update_layout();
                        cols.back().to<Layout>()->auto_size();
                        
                        print("Col:");
                        for(auto& o : cols.back().to<Layout>()->objects())
                            print("\t",*o, " @ ", o->bounds());
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
    
    auto vertical_clr = get(_defaults.verticalClr, "vertical_clr");
    auto horizontal_clr = get(_defaults.horizontalClr, "horizontal_clr");
    
    auto ptr = Layout::Make<GridLayout>(valign, halign, std::move(rows), VerticalClr{vertical_clr}, HorizontalClr{horizontal_clr});

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
    
    std::string key = var + std::to_string(hash);
    {
        {
            if(not state._text_fields.contains(key) or not state._text_fields.at(key)) {
                auto ptr = LabeledField::Make(var, obj, invert);
                state._text_fields.emplace(key, std::move(ptr));
            } else {
                throw U_EXCEPTION("Cannot deal with replacement LabeledFields yet.");
                //state._text_fields.at(key) = std::move(ptr);
            }
        }
        
        auto& ref = state._text_fields.at(key);
        if(not ref) {
            FormatWarning("Cannot create representative of field ", var, " when creating controls for type ",settings_map()[var].get().type_name(),".");
            return nullptr;
        }
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
        if(max_size != Vec2(0)) 
            ref->set(attr::SizeLimit{max_size});
        
        Color color{_defaults.textClr};
        if(obj.count("color")) {
            if(obj["color"].is_string())
                state.patterns[hash]["color"] = obj["color"].get<std::string>();
            else
                color = parse_color(obj["color"]);
        }
        ref->set(attr::TextClr{color});
        
        Color highlight_clr{_defaults.highlightClr};
        if(obj.count("highlight_clr")) {
            if(obj["highlight_clr"].is_string())
                state.patterns[hash]["highlight_clr"] = obj["highlight_clr"].get<std::string>();
            else
                highlight_clr = parse_color(obj["highlight_clr"]);
        }
        ref->set(attr::HighlightClr{highlight_clr});
        
        std::vector<Layout::Ptr> objs;
        ref->add_to(objs);
        for(auto &o : objs)
            assert(o != nullptr);
        ptr = Layout::Make<HorizontalLayout>(std::move(objs), Loc(), Box{0, 0, 0, 0}, Margins{0,0,0,0});
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
    
    auto ptr = Layout::Make<Button>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr->on_click([action, context](auto){
            if(context.actions.contains(action))
                context.actions.at(action)(action);
            else
                print("Unknown Action: ", action);
        });
    }
    
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
    
    auto ptr = Layout::Make<Checkbox>(attr::Str(text), attr::Checked(checked), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr.to<Checkbox>()->on_change([action, context](){
            if(context.actions.contains(action))
                context.actions.at(action)(action);
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
    
    auto ptr = Layout::Make<Textfield>(attr::Str(text), attr::Scale(scale), attr::Origin(origin), font);
    
    std::string action;
    if(obj.count("action")) {
        action = obj["action"].get<std::string>();
        ptr.to<Textfield>()->on_enter([action, context](){
            if(context.actions.contains(action))
                context.actions.at(action)(action);
            else
                print("Unknown Action: ", action);
        });
    }
    
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
    
    if(utils::contains(text, '{')) {
        state.patterns[hash]["text"] = text;
    }
    
    StaticText::FadeOut_t fade_out{false};
    if(obj.contains("fade_out") && obj["fade_out"].is_boolean()) {
        fade_out = obj["fade_out"].get<bool>();
    }
    
    ptr = Layout::Make<StaticText>(Str{text}, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font, fade_out);
    
    //if(margins != Margins{0, 0, 0, 0})
    {
        ptr.to<StaticText>()->set(attr::Margins(margins));
        if(not max_size.empty())
            ptr.to<StaticText>()->set(attr::SizeLimit{max_size});
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
    
    ptr = Layout::Make<Text>(Str{text}, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
    ptr->set_name(text);
    
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
            ptr.to<ScrollableList<DetailItem>>()->on_select([&, &body = state.lists[hash]](size_t index, const DetailItem & item)
            {
                if(body.on_select_actions.contains(index)) {
                    std::get<1>(body.on_select_actions.at(index))();
                }
            });
            ptr->set_name("list<"+obj["var"].get<std::string>()+" "+Meta::toStr(hash)+">");
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
