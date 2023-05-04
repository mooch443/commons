#include "DynamicGUI.h"
#include <gui/types/Button.h>
#include <gui/types/StaticText.h>
#include <misc/SpriteMap.h>
#include <file/DataLocation.h>

namespace gui {
namespace dyn {

Variable var([](int) -> int{
    return 0;
});

void foo() {
    var.value_string(5);
}

namespace Modules {
std::unordered_map<std::string, Module> mods;

void add(Module&& m) {
    auto name = m._name;
    mods.emplace(name, std::move(m));
}

Module* exists(const std::string& name) {
    auto it = mods.find(name);
    if(it != mods.end()) {
        return &it->second;
    }
    return nullptr;
}

}

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state)
{
    auto index = state.object_index++;
    auto type = LayoutType::get(utils::lowercase(obj["type"].get<std::string>()));
    
    auto get = [&]<typename T>(T de, auto name)->T {
        if(obj.count(name)) {
            //if constexpr(std::same_as<T, Color>)
            {
                // is it a color variable?
                if(obj[name].is_string()) {
                    state.patterns[index][name] = obj[name].template get<std::string>();
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
                    //print("collection: ", child.dump());
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
            if(line != Transparent)
                ptr.to<Rect>()->set_lineclr(line);
            
            break;
        }
            
        case LayoutType::each: {
            if(obj.count("do")) {
                auto child = obj["do"];
                if(obj.count("var") && obj["var"].is_string() && child.is_object()) {
                    //print("collection: ", child.dump());
                    // all successfull, add collection:
                    ptr = Layout::Make<Layout>(std::vector<Layout::Ptr>{});
                    state.loops[index] = {
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
                    
                    auto c = parse_object(child, context, state);
                    c->set_is_displayed(false);
                    
                    Layout::Ptr _else;
                    if(obj.count("else") && obj["else"].is_object()) {
                        _else = parse_object(obj["else"], context, state);
                    }
                    
                    state.ifs[index] = IfBody{
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
                auto m = Modules::exists(mod);
                if(m)
                    m->_apply(index, state, ptr);
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
                    output += resolve_variable(word, context, [word](const VarBase_t& variable, const std::string& modifiers, bool optional) -> std::string {
                        try {
                            auto ret = variable.value_string(modifiers);
                            //print(word, " resolves to ", ret);
                            return ret;
                        } catch(...) {
                            return optional ? "" : "null";
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
    auto index = (size_t)o->custom_data("object_index");
    
    //! something that needs to be executed before everything runs
    if(state.display_fns.contains(index)) {
        state.display_fns.at(index)(g);
    }
    
    //! if statements below
    if(auto it = state.ifs.find(index); it != state.ifs.end()) {
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
    if(auto it = state.loops.find(index); it != state.loops.end()) {
        auto &obj = it->second;
        if(context.variables.contains(obj.variable)) {
            if(context.variables.at(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                
                auto& vector = context.variables.at(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>("");
                
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
                        update_objects(g, o.to<Layout>()->children()[i], tmp, *obj.state);
                    }
                }
            }
        }
        return;
    }
    
    //! fill default fields like fill, line, pos, etc.
    auto it = state.patterns.find(index);
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
                auto pos = resolve_variable_type<Size2>(pattern.at("size"), context);
                o->set_size(pos);
                
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

}
}
