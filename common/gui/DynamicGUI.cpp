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
#include <regex>

namespace gui {
namespace dyn {

std::string extractControls(std::string& variable) {
    std::string controls;
    std::size_t controlsSize = 0;
    for (char c : variable) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == ':' || c == ' ') {
            break;
        }
        controls += c;
        controlsSize++;
    }

    if (controlsSize > 0) {
        variable = variable.substr(controlsSize);
    }

    return controls;
}

// New function to handle the regex related work
std::string regExtractControls(std::string& variable) {
    std::regex rgx("[^a-zA-Z0-9_\\-: ]+");
    std::smatch match;
    std::string controls;
    if (std::regex_search(variable, match, rgx)) {
        controls = match[0].str();
        variable = variable.substr(controls.size());
    }
    return controls;
}

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

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state)
{
    try {
        auto hash = state._current_index.hash();
        print("Calculating hash for index ", hash, " at ", state._current_index.id_chain);
        
        state._current_index.inc();
        
        auto type_name = utils::lowercase(obj["type"].get<std::string>());
        auto type = LayoutType::get(type_name);
        
        auto get = [&]<typename T>(T de, auto name)->T {
            if(obj.count(name)) {
                //if constexpr(std::same_as<T, Color>)
                {
                    // is it a color variable?
                    if(obj[name].is_string()) {
                        state.patterns[hash][name] = obj[name].template get<std::string>();
                        //print("pattern for ", name, " at object ", obj[name].template get<std::string>());
                        //std::cout << obj << std::endl;
                        return de;
                    }
                }
                return Meta::fromStr<T>(obj[name].dump());
            }
            return de;
        };
        
        auto scale = get(Vec2(1), "scale");
        auto size = get(Size2(0), "size");
        auto pos = get(Vec2(0), "pos");
        auto origin = get(Vec2(0), "origin");
        auto margins = get(Bounds(5,5,5,5), "margins");
        auto name = get(std::string(), "name");
        
        auto fill = get(Transparent, "fill");
        auto line = get(Transparent, "line");
        
        auto clickable = get(false, "clickable");
        
        Font font(0.75);
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
        
        Layout::Ptr ptr;
        
        switch (type) {
            case LayoutType::combobox: {
                
                break;
            }
            case LayoutType::image: {
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
                    ptr = Layout::Make<ExternalImage>(std::move(img));
                } else {
                    ptr = Layout::Make<ExternalImage>();
                }
                
                break;
            }
            case LayoutType::vlayout: {
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
                
                ptr = Layout::Make<VerticalLayout>(std::move(children), pos, margins);
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
                break;
            }
                
            case LayoutType::hlayout: {
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
                
                ptr = Layout::Make<HorizontalLayout>(std::move(children), pos, margins);
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
                break;
            }
                
            case LayoutType::gridlayout: {
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
                                        std::cout << obj << std::endl;
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
                
                ptr = Layout::Make<GridLayout>();
                ptr.to<GridLayout>()->set_children(std::move(rows));
                
                ptr.to<GridLayout>()->update_layout();
                auto str = ptr.to<GridLayout>()->toString(nullptr);
                print(str);
                print(ptr.to<GridLayout>()->grid_info());
                
                
                auto vertical_clr = get(DarkCyan.alpha(150), "vertical_clr");
                auto horizontal_clr = get(DarkGray.alpha(150), "horizontal_clr");
                
                ptr.to<GridLayout>()->set(VerticalClr{vertical_clr});
                ptr.to<GridLayout>()->set(HorizontalClr{horizontal_clr});
                
                /*if(obj.count("align")) {
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
                 }*/
                break;
            }
                
            case LayoutType::collection: {
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
                
                ptr = Layout::Make<Layout>(std::move(children));
                ptr->set_size(size);
                ptr->set_pos(pos);
                ptr.to<Layout>()->set(FillClr{fill});
                ptr.to<Layout>()->set(LineClr{line});
                break;
            }
                
            case LayoutType::settings: {
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
                
                break;
            }
                
            case LayoutType::button: {
                std::string text;
                if(obj.contains("text")) {
                    text = obj["text"].get<std::string>();
                }
                
                if(utils::contains(text, '{')) {
                    state.patterns[hash]["text"] = text;
                }
                
                ptr = Layout::Make<Button>(text, attr::Scale(scale), attr::Origin(origin), font);
                
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
                
                break;
            }
                
            case LayoutType::checkbox: {
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
                
                ptr = Layout::Make<Checkbox>(attr::Loc(), text, attr::Checked(checked), font);
                
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
                
                break;
            }
                
            case LayoutType::textfield: {
                std::string text;
                if(obj.contains("text")) {
                    text = obj["text"].get<std::string>();
                }
                
                if(utils::contains(text, '{')) {
                    state.patterns[hash]["text"] = text;
                }
                
                ptr = Layout::Make<Textfield>(attr::Content(text), attr::Scale(scale), attr::Origin(origin), font);
                
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
                
                break;
            }
                
            case LayoutType::text: {
                std::string text;
                if(obj.contains("text")) {
                    text = obj["text"].get<std::string>();
                }
                
                if(utils::contains(text, '{')) {
                    state.patterns[hash]["text"] = text;
                }
                
                ptr = Layout::Make<Text>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
                
                Color color{White};
                if(obj.count("color")) {
                    if(obj["color"].is_string())
                        state.patterns[hash]["color"] = obj["color"].get<std::string>();
                    else {
                        color = parse_color(obj["color"]);
                        ptr.to<Text>()->set_color(color);
                    }
                }
                
                break;
            }
                
            case LayoutType::stext: {
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
                
                break;
            }
                
            case LayoutType::rect: {
                ptr = Layout::Make<Rect>(attr::Scale(scale), attr::Loc(pos), attr::Size(size), attr::Origin(origin));
                
                if(fill != Transparent)
                    ptr.to<Rect>()->set_fillclr(fill);
                if(line != Transparent)
                    ptr.to<Rect>()->set_lineclr(line);
                
                break;
            }
                
            case LayoutType::circle: {
                float radius = 0;
                if(obj.count("radius")) {
                    if(obj["radius"].is_number())
                        radius = obj["radius"].get<float>();
                }
                ptr = Layout::Make<Circle>(attr::Scale(scale), attr::Loc(pos), attr::Radius(radius), attr::Origin(origin), attr::FillClr{fill}, attr::LineClr{line});
                break;
            }
                
            case LayoutType::each: {
                if(obj.count("do")) {
                    auto child = obj["do"];
                    if(obj.count("var") && obj["var"].is_string() && child.is_object()) {
                        //print("collection: ", child.dump());
                        // all successfull, add collection:
                        ptr = Layout::Make<Layout>(std::vector<Layout::Ptr>{});
                        state.loops[hash] = {
                            .variable = obj["var"].get<std::string>(),
                            .child = child,
                            .state = std::make_unique<State>()
                        };
                    }
                }
                break;
            }
                
            case LayoutType::condition: {
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
                break;
            }
                
            default:
                std::cout << obj << std::endl;
                break;
        }
        
        if(not ptr) {
            FormatExcept("Cannot create object at ",hash, " ",obj["type"].get<std::string>());
            return nullptr;
        }
        
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
        print("adding object_index to ",ptr->type()," with index ", hash, " for ", (uint64_t)ptr.get());
        return ptr;
    } catch(const std::exception& e) {
        print("Caught exception here: ", e.what());
    }
}

std::string parse_text(const std::string& pattern, const Context& context) {
    int inside = 0;
    bool comment_out = false;
    std::string word, output;
    for(size_t i = 0; i<pattern.length(); ++i) {
        if(inside == 0) {
            if(pattern[i] == '\\') {
                if(not comment_out)
                    comment_out = true;
            } else if(comment_out) {
                output += pattern[i];
                comment_out = false;
                
            } else if(pattern[i] == '{') {
                word = "";
                inside++;
                
            } else {
                output += pattern[i];
            }
            
        } else {
            if(pattern[i] == '}') {
                inside--;
                
                if(inside == 0) {
                    output += resolve_variable(word, context, [word](const VarBase_t& variable, const VarProps& modifiers) -> std::string {
                        try {
                            auto ret = variable.value_string(modifiers.sub);
                            //print(word, " resolves to ", ret);
                            if(modifiers.html)
                                return settings::htmlify(ret);
                            return ret;
                        } catch(...) {
                            return modifiers.optional ? "" : "null";
                        }
                    }, [word](bool optional) -> std::string {
                        return optional ? "" : "null";
                    });
                }
                
            } else
                word += pattern[i];
        }
    }
    return output;
}

void update_objects(DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state) {
    auto hash = (std::size_t)o->custom_data("object_index");
    print("updating ", o->type()," with index ", hash, " for ", (uint64_t)o.get(), " ", o->name());
    
    //! something that needs to be executed before everything runs
    if(state.display_fns.contains(hash)) {
        state.display_fns.at(hash)(g);
    }
    
    //! if statements below
    if(auto it = state.ifs.find(hash); it != state.ifs.end()) {
        auto &obj = it->second;
        try {
            auto res = resolve_variable_type<bool>(obj.variable, context);
            if(not res) {
                if(obj._else) {
                    auto last_condition = (uint64_t)o->custom_data("last_condition");
                    if(last_condition != 1) {
                        o->add_custom_data("last_condition", (void*)1);
                        o.to<Layout>()->set_children({obj._else});
                    }
                    update_objects(g, obj._else, context, state);
                    
                } else {
                    if(o->is_displayed()) {
                        o->set_is_displayed(false);
                        o.to<Layout>()->clear_children();
                    }
                }
                
                return;
            }
            
            auto last_condition = (uint64_t)o->custom_data("last_condition");
            if(last_condition != 2) {
                o->set_is_displayed(true);
                o->add_custom_data("last_condition", (void*)2);
                o.to<Layout>()->set_children({obj._if});
            }
            update_objects(g, obj._if, context, state);
            
        } catch(...) { }
        
        return;
    }
    
    //! for-each loops below
    if(auto it = state.loops.find(hash); it != state.loops.end()) {
        auto &obj = it->second;
        if(context.variables.contains(obj.variable)) {
            if(context.variables.at(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                
                auto& vector = context.variables.at(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>("");
                
                IndexScopeHandler handler{state._current_index};
                if(vector != obj.cache) {
                    std::vector<Layout::Ptr> ptrs;
                    obj.cache = vector;
                    obj.state = std::make_unique<State>(state);
                    Context tmp = context;
                    
                    for(auto &v : vector) {
                        tmp.variables["i"] = v;
                        auto ptr = parse_object(obj.child, tmp, *obj.state);
                        update_objects(g, ptr, tmp, *obj.state);
                        ptrs.push_back(ptr);
                    }
                    
                    o.to<Layout>()->set_children(ptrs);
                    
                } else {
                    Context tmp = context;
                    for(size_t i=0; i<obj.cache.size(); ++i) {
                        tmp.variables["i"] = obj.cache[i];
                        //print("Setting i", i," to ", tmp.variables["i"]->value_string("pos"), " for ",o.to<Layout>()->children()[i]);
                        auto p = o.to<Layout>()->children().at(i);
                        if(p)
                            update_objects(g, p, tmp, *obj.state);
                    }
                }
            }
        }
        return;
    }
    
    //! fill default fields like fill, line, pos, etc.
    auto it = state.patterns.find(hash);
    if(it != state.patterns.end()) {
        auto pattern = it->second;
        
        if(it->second.contains("fill")) {
            try {
                auto fill = resolve_variable_type<Color>(pattern.at("fill"), context);
                if(o.is<Rect>())
                    o.to<Rect>()->set_fillclr(fill);
                else if(o.is<SectionInterface>())
                    o.to<SectionInterface>()->set_background(fill, o.to<SectionInterface>()->background()->lineclr());
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("line")) {
            try {
                auto line = resolve_variable_type<Color>(pattern.at("line"), context);
                if(o.is<Rect>())
                    o.to<Rect>()->set_lineclr(line);
                else if(o.is<SectionInterface>())
                    o.to<SectionInterface>()->set_background(o.to<SectionInterface>()->background()->fillclr(), line);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        
        if(pattern.contains("pos")) {
            try {
                auto pos = resolve_variable_type<Vec2>(pattern.at("pos"), context);
                o->set_pos(pos);
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), ")");
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("size")) {
            try {
                if(pattern.at("size") == "auto"
                   && o.is<Layout>()) 
                {
                    o.to<Layout>()->auto_size({});
                } else {
                    print("pattern.at(size) = ", pattern.at("size"));
                    print("is layout = ", o.is<Layout>());
                    print("o = ", o.get());
                    auto pos = resolve_variable_type<Size2>(pattern.at("size"), context);
                    o->set_size(pos);
                }
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
    }
    
    //! if this is a Layout type, need to iterate all children as well:
    if(o.is<Layout>()) {
        for(auto &child : o.to<Layout>()->objects()) {
            update_objects(g, child, context, state);
        }
    }
    
    //! fill other properties specific to type:
    if(it != state.patterns.end()) {
        auto pattern = it->second;
        
        if(o.is<Text>()) {
            if(pattern.contains("text")) {
                auto text = o.to<Text>();
                auto output = parse_text(pattern.at("text"), context);
                text->set_txt(output);
            }
            
        } else if(o.is<StaticText>()) {
            if(pattern.contains("text")) {
                auto text = o.to<StaticText>();
                auto output = parse_text(pattern.at("text"), context);
                text->set_txt(output);
            }
            
        } else if(o.is<Button>()) {
            if(pattern.contains("text")) {
                auto button = o.to<Button>();
                auto output = parse_text(pattern.at("text"), context);
                button->set_txt(output);
            }
        } else if(o.is<Textfield>()) {
            if(pattern.contains("text")) {
                auto textfield = o.to<Textfield>();
                auto output = parse_text(pattern.at("text"), context);
                textfield->set_text(output);
            }
        } else if(o.is<Checkbox>()) {
            if(pattern.contains("text")) {
                auto checkbox = o.to<Checkbox>();
                auto output = parse_text(pattern.at("text"), context);
                checkbox->set_text(output);
            }
        } else if(o.is<ExternalImage>()) {
            if(pattern.contains("path")) {
                auto img = o.to<ExternalImage>();
                auto output = file::Path(parse_text(pattern.at("path"), context));
                if(output.exists() && not output.is_folder()) {
                    auto modified = output.last_modified();
                    auto &entry = state._image_cache[output.str()];
                    Image::Ptr ptr;
                    if(std::get<0>(entry) != modified) {
                        ptr = load_image(output);
                        entry = { modified, Image::Make(*ptr) };
                    }
                    
                    if(ptr) {
                        img->set_source(std::move(ptr));
                    }
                }
            }
        }
    }
}

void update_layout(const file::Path& path, Context& context, State& state, std::vector<Layout::Ptr>& objects) {
    try {
        auto cwd = file::cwd().absolute();
        auto app = file::DataLocation::parse("app").absolute();
        if(cwd != app) {
            print("check_module:CWD: ", cwd);
            file::cd(app);
        }
        
        auto result = load(path);
        if(result) {
            state._text_fields.clear();
            
            auto layout = result.value();
            std::vector<Layout::Ptr> objs;
            State tmp;
            for(auto &obj : layout) {
                auto ptr = parse_object(obj, context, tmp);
                if(ptr) {
                    objs.push_back(ptr);
                }
            }
            
            state = std::move(tmp);
            objects = std::move(objs);
        }
        
    } catch(const std::invalid_argument&) {
    } catch(const std::exception& e) {
        FormatExcept("Error loading gui layout from file: ", e.what());
    } catch(...) {
        FormatExcept("Error loading gui layout from file");
    }
}

void update_tooltips(DrawStructure& graph, State& state) {
    if(not state._settings_tooltip) {
        state._settings_tooltip = Layout::Make<SettingsTooltip>();
    }
    
    Layout::Ptr found = nullptr;
    std::string name;
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto & [key, ptr] : state._text_fields) {
        ptr->_text->set_clickable(true);
        
        if(ptr->representative()->hovered() && (ptr->representative().ptr.get() == graph.hovered_object() || dynamic_cast<Textfield*>(graph.hovered_object()))) {
            found = ptr->representative();
            name = key;
            break;
        }
    }
    
    if(found) {
        ref = std::make_unique<sprite::Reference>(dyn::settings_map()[name]);
    }

    if(found && ref) {
        state._settings_tooltip.to<SettingsTooltip>()->set_parameter(name);
        state._settings_tooltip.to<SettingsTooltip>()->set_other(found.get());
        graph.wrap_object(*state._settings_tooltip);
    } else {
        state._settings_tooltip.to<SettingsTooltip>()->set_other(nullptr);
    }
}

}
}
