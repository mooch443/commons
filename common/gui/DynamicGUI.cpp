#include "DynamicGUI.h"
#include <gui/types/Button.h>

namespace gui {
namespace dyn {

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state)
{
    auto index = state.object_index++;
    auto type = LayoutType::get(utils::lowercase(obj["type"].get<std::string>()));
    Vec2 scale(1);
    if(obj.count("scale")) {
        auto scales = obj["scale"].get<std::vector<float>>();
        assert(scales.size() == 2);
        scale = Vec2(scales[0], scales[1]);
    }
    
    Size2 size(0);
    if(obj.count("size")) {
        auto scales = obj["size"].get<std::vector<float>>();
        assert(scales.size() == 2);
        size = Size2(scales[0], scales[1]);
    }
    
    Vec2 pos(0);
    if(obj.count("pos")) {
        auto scales = obj["pos"].get<std::vector<float>>();
        assert(scales.size() == 2);
        pos = Vec2(scales[0], scales[1]);
    }
    
    Vec2 origin(0);
    if(obj.count("origin")) {
        auto scales = obj["origin"].get<std::vector<float>>();
        assert(scales.size() == 2);
        origin = Vec2(scales[0], scales[1]);
    }
    
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
            for(auto &child : obj["children"]) {
                auto ptr = parse_object(child, context, state);
                if(ptr) {
                    children.push_back(ptr);
                }
            }
            
            ptr = Layout::Make<VerticalLayout>(std::move(children));
            assert(ptr.is<VerticalLayout>());
            break;
        }
            
        case LayoutType::hlayout: {
            std::vector<Layout::Ptr> children;
            for(auto &child : obj["children"]) {
                auto ptr = parse_object(child, context, state);
                if(ptr) {
                    children.push_back(ptr);
                }
            }
            
            ptr = Layout::Make<HorizontalLayout>(std::move(children));
            break;
        }
            
        case LayoutType::button: {
            auto text = obj["text"].get<std::string>();
            if(utils::contains(text, '{')) {
                state.patterns[index] = text;
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
                    state.patterns[index] = obj["color"].get<std::string>();
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
                state.patterns[index] = text;
            }
            
            ptr = Layout::Make<Text>(text, attr::Scale(scale), attr::Loc(pos), attr::Origin(origin), font);
            
            Color color{White};
            if(obj.count("color")) {
                if(obj["color"].is_string())
                    state.patterns[index] = obj["color"].get<std::string>();
                else {
                    color = parse_color(obj["color"]);
                    ptr.to<Text>()->set_color(color);
                }
            }
            
            break;
        }
            
        case LayoutType::rect: {
            ptr = Layout::Make<Rect>(attr::Scale(scale), attr::Loc(pos), attr::Size(size), attr::Origin(origin));
            
            Color color{Transparent};
            if(obj.count("color")) {
                if(obj["color"].is_string())
                    state.patterns[index] = obj["color"].get<std::string>();
                else {
                    color = parse_color(obj["color"]);
                    ptr.to<Rect>()->set_fillclr(color);
                }
            }
            
            break;
        }
            
        default:
            std::cout << obj << std::endl;
            break;
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
                auto parts = utils::split(word, ':');
                auto variable = utils::lowercase(parts.front());
                std::string modifiers;
                if(parts.size() > 1)
                    modifiers = parts.back();
                
                if(context.variables.contains(variable)) {
                    output += context.variables.at(variable)(modifiers);
                } else
                    output += "null";
                
                inside = false;
            } else
                word += pattern[i];
        }
    }
    return output;
}

void update_objects(const Layout::Ptr& o, const Context& context, const State& state) {
    auto index = (size_t)o->custom_data("object_index");
    
    if(o.is<HorizontalLayout>()) {
        for(auto &child : o.to<HorizontalLayout>()->objects()) {
            update_objects(child, context, state);
        }
    } else if(o.is<VerticalLayout>()) {
        for(auto &child : o.to<VerticalLayout>()->objects()) {
            update_objects(child, context, state);
        }
    } else if(o.is<Text>()) {
        auto text = o.to<Text>();
        auto it = state.patterns.find(index);
        if(it != state.patterns.end()) {
            auto pattern = it->second;
            auto output = parse_text(pattern, context);
            text->set_txt(output);
        }
    } else if(o.is<Button>()) {
        auto button = o.to<Button>();
        auto it = state.patterns.find(index);
        if(it != state.patterns.end()) {
            auto pattern = it->second;
            auto output = parse_text(pattern, context);
            button->set_txt(output);
        }
    } else if(o.is<Rect>()) {
        auto rect = o.to<Rect>();
        auto it = state.patterns.find(index);
        if(it != state.patterns.end()) {
            auto pattern = utils::lowercase(it->second);
            if(context.color_variables.contains(pattern)) {
                rect->set_fillclr(context.color_variables.at(pattern)());
            }
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
