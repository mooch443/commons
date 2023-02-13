#include "DynamicGUI.h"
#include <gui/types/Button.h>
#include <gui/types/StaticText.h>
#include <misc/SpriteMap.h>

namespace gui {
namespace dyn {

Variable var([](int) -> int{
    return 0;
});

void foo() {
    var.value_string(5);
}

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state)
{
    auto index = state.object_index++;
    auto type = LayoutType::get(utils::lowercase(obj["type"].get<std::string>()));
    
    
    struct Module {
        const std::string name;
        void apply(size_t index, State& state, const Layout::Ptr& obj) {
            if(name == "follow") {
                state.display_fns[index] = [o = obj.get()](DrawStructure& g){
                    o->set_pos(g.mouse_position() + Vec2(5));
                };
            } else if(name == "infocard") {
                if(obj.is<Layout>()) {
                    auto layout = obj.to<Layout>();
                    layout->set_children({
                        Layout::Make<Text>("Text")
                    });
                }
            }
        }
    };
    
    auto get = [&]<typename T>(T de, auto name)->T {
        if(obj.count(name)) {
            //if constexpr(std::same_as<T, Color>)
            {
                // is it a color variable?
                if(obj[name].is_string()) {
                    state.patterns[index][name] = obj[name].template get<std::string>();
                    print("pattern for ", name, " at object ", obj[name].template get<std::string>());
                    std::cout << obj << std::endl;
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
    
    Font font;
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
    }
    
    Layout::Ptr ptr;
    
    switch (type) {
        case LayoutType::vlayout: {
            std::vector<Layout::Ptr> children;
            if(obj.count("children")) {
                for(auto &child : obj["children"]) {
                    auto ptr = parse_object(child, context, state);
                    if(ptr) {
                        children.push_back(ptr);
                    }
                }
            }
            
            ptr = Layout::Make<VerticalLayout>(std::move(children), pos, margins);
            break;
        }
            
        case LayoutType::hlayout: {
            std::vector<Layout::Ptr> children;
            
            if(obj.count("children")) {
                for(auto &child : obj["children"]) {
                    auto ptr = parse_object(child, context, state);
                    if(ptr) {
                        children.push_back(ptr);
                    }
                }
            }
            
            ptr = Layout::Make<HorizontalLayout>(std::move(children), pos, margins);
            break;
        }
            
        case LayoutType::collection: {
            std::vector<Layout::Ptr> children;
            
            if(obj.count("children")) {
                for(auto &child : obj["children"]) {
                    print("collection: ", child.dump());
                    auto ptr = parse_object(child, context, state);
                    if(ptr) {
                        children.push_back(ptr);
                    }
                }
            }
            
            ptr = Layout::Make<Layout>(std::move(children));
            break;
        }
            
        case LayoutType::button: {
            auto text = obj["text"].get<std::string>();
            if(utils::contains(text, '{')) {
                state.patterns[index]["text"] = text;
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
                    state.patterns[index]["color"] = obj["color"].get<std::string>();
                else {
                    color = parse_color(obj["color"]);
                    ptr.to<Button>()->set_text_clr(color);
                }
            }
            
            break;
        }
            
        case LayoutType::text: {
            auto text = obj["text"].get<std::string>();
            if(utils::contains(text, '{')) {
                state.patterns[index]["text"] = text;
            }
            
            ptr = Layout::Make<Text>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
            
            Color color{White};
            if(obj.count("color")) {
                if(obj["color"].is_string())
                    state.patterns[index]["color"] = obj["color"].get<std::string>();
                else {
                    color = parse_color(obj["color"]);
                    ptr.to<Text>()->set_color(color);
                }
            }
            
            break;
        }
            
        case LayoutType::stext: {
            auto text = obj["text"].get<std::string>();
            if(utils::contains(text, '{')) {
                state.patterns[index]["text"] = text;
            }
            
            ptr = Layout::Make<StaticText>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
            
            Color color{White};
            if(obj.count("color")) {
                if(obj["color"].is_string())
                    state.patterns[index]["color"] = obj["color"].get<std::string>();
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
            
            break;
        }
            
        default:
            std::cout << obj << std::endl;
            break;
    }
    
    if(ptr.is<SectionInterface>()) {
        if(fill != Transparent) {
            if(line != Transparent)
                ptr.to<SectionInterface>()->set_background(fill, line);
            else
                ptr.to<SectionInterface>()->set_background(fill);
        } else if(line != Transparent)
            ptr.to<SectionInterface>()->set_background(Transparent, line);
    }
    
    if(not name.empty())
        ptr->set_name(name);
    
    if(obj.count("modules")) {
        if(obj["modules"].is_array()) {
            for(auto mod : obj["modules"]) {
                if(mod == "draggable") {
                    ptr->set_draggable();
                } else {
                    Module{mod}.apply(index, state, ptr);
                }
            }
        }
    }
    
    if(scale != Vec2(1)) ptr->set_scale(scale);
    if(pos != Vec2(0)) ptr->set_pos(pos);
    if(size != Vec2(0)) ptr->set_size(size);
    if(origin != Vec2(0)) ptr->set_origin(origin);
    ptr->add_custom_data("object_index", (void*)index);
    return ptr;
}

std::string parse_text(const std::string& pattern, const Context& context) {
    bool inside = false;
    std::string word, output;
    for(size_t i = 0; i<pattern.length(); ++i) {
        if(not inside) {
            if(pattern[i] == '{') {
                word = "";
                inside = true;
            } else {
                output += pattern[i];
            }
            
        } else {
            if(pattern[i] == '}') {
                output += resolve_variable(word, context, [](const VarBase_t& variable, const std::string& modifiers, bool optional) -> std::string {
                    try {
                        return variable.value_string(modifiers);
                    } catch(...) {
                        return optional ? "" : "null";
                    }
                }, [word](bool optional) -> std::string {
                    print("word=",word," ","optional=",optional);
                    return optional ? "" : "null";
                });
                
                inside = false;
            } else
                word += pattern[i];
        }
    }
    return output;
}

void update_objects(DrawStructure& g, const Layout::Ptr& o, const Context& context, const State& state) {
    auto index = (size_t)o->custom_data("object_index");
    
    if(state.display_fns.contains(index)) {
        state.display_fns.at(index)(g);
    }
    
    auto it = state.patterns.find(index);
    if(it != state.patterns.end()) {
        auto pattern = it->second;
        
        if(it->second.contains("fill")) {
            auto pattern = utils::lowercase(it->second.at("fill"));
            if(context.variables.contains(pattern)) {
                parse_text(pattern, context);
            }
            if(context.color_variables.contains(pattern)) {
                if(o.is<Rect>())
                    o.to<Rect>()->set_fillclr(context.color_variables.at(pattern)());
                else if(o.is<SectionInterface>())
                    o.to<SectionInterface>()->set_background(context.color_variables.at(pattern)(), o.to<SectionInterface>()->background()->lineclr());
            }
        }
        if(it->second.contains("line")) {
            auto pattern = utils::lowercase(it->second.at("line"));
            if(context.color_variables.contains(pattern)) {
                if(o.is<Rect>())
                    o.to<Rect>()->set_lineclr(context.color_variables.at(pattern)());
                else if(o.is<SectionInterface>())
                    o.to<SectionInterface>()->set_background(o.to<SectionInterface>()->background()->fillclr(), context.color_variables.at(pattern)());
            }
        }
        
        
        if(pattern.contains("pos")) {
            try {
                auto pos = resolve_variable_type<Vec2>(pattern.at("pos"), context);
                o->set_pos(pos);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("size")) {
            try {
                auto pos = resolve_variable_type<Size2>(pattern.at("size"), context);
                o->set_size(pos);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
    }
    
    if(o.is<Layout>()) {
        for(auto &child : o.to<Layout>()->objects()) {
            update_objects(g, child, context, state);
        }
    }
    
    if(it != state.patterns.end()) {
        auto pattern = it->second;
        
        if(o.is<Text>()) {
            auto text = o.to<Text>();
            if(pattern.contains("text")) {
                auto output = parse_text(pattern.at("text"), context);
                text->set_txt(output);
            }
            
        } else if(o.is<StaticText>()) {
            auto text = o.to<StaticText>();
            if(pattern.contains("text")) {
                auto output = parse_text(pattern.at("text"), context);
                text->set_txt(output);
            }
            
        } else if(o.is<Button>()) {
            auto button = o.to<Button>();
            if(pattern.contains("text")) {
                auto output = parse_text(pattern.at("text"), context);
                button->set_txt(output);
            }
        } else if(o.is<Rect>()) {
            auto rect = o.to<Rect>();
        }
    }
}

void update_layout(const file::Path& path, Context& context, State& state, std::vector<Layout::Ptr>& objects) {
    try {
        auto layout = load(path);
        
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
        
    } catch(const std::invalid_argument&) {
    } catch(const std::exception& e) {
        FormatExcept("Error loading gui layout from file: ", e.what());
    } catch(...) {
        FormatExcept("Error loading gui layout from file");
    }
}

}
}
