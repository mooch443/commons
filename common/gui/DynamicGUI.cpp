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
#include <gui/ParseLayoutTypes.h>
#include <gui/types/ErrorElement.h>
#include <gui/DrawBase.h>
#include <regex>

namespace gui {
namespace dyn {

void update_objects(DrawStructure&, const Layout::Ptr& o, const Context& context, State& state);

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

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state,
                         const DefaultSettings& defaults)
{
    LayoutContext layout(obj, state, defaults);
    try {
        
        Layout::Ptr ptr;

        switch (layout.type) {
            case LayoutType::each:
                ptr = layout.create_object<LayoutType::each>(context);
                break;
            case LayoutType::list:
                ptr = layout.create_object<LayoutType::list>(context);
                break;
            case LayoutType::condition:
                ptr = layout.create_object<LayoutType::condition>(context);
                break;
            case LayoutType::vlayout:
                ptr = layout.create_object<LayoutType::vlayout>(context);
                break;
            case LayoutType::hlayout:
                ptr = layout.create_object<LayoutType::hlayout>(context);
                break;
            case LayoutType::gridlayout:
                ptr = layout.create_object<LayoutType::gridlayout>(context);
                break;
            case LayoutType::collection:
                ptr = layout.create_object<LayoutType::collection>(context);
                break;
            case LayoutType::button:
                ptr = layout.create_object<LayoutType::button>(context);
                break;
            case LayoutType::circle:
                ptr = layout.create_object<LayoutType::circle>(context);
                break;
            case LayoutType::text:
                ptr = layout.create_object<LayoutType::text>(context);
                break;
            case LayoutType::stext:
                ptr = layout.create_object<LayoutType::stext>(context);
                break;
            case LayoutType::rect:
                ptr = layout.create_object<LayoutType::rect>(context);
                break;
            case LayoutType::textfield:
                ptr = layout.create_object<LayoutType::textfield>(context);
                break;
            case LayoutType::checkbox:
                ptr = layout.create_object<LayoutType::checkbox>(context);
                break;
            case LayoutType::settings:
                ptr = layout.create_object<LayoutType::settings>(context);
                break;
            case LayoutType::combobox:
                ptr = layout.create_object<LayoutType::combobox>(context);
                break;
            case LayoutType::image:
                ptr = layout.create_object<LayoutType::image>(context);
                break;
            default:
                FormatExcept("Unknown layout type: ", layout.type);
                break;
        }
        
        if(not ptr) {
            FormatExcept("Cannot create object ",obj["type"].get<std::string>());
            return nullptr;
        }
        
        layout.finalize(ptr);
        return ptr;
    } catch(const std::exception& e) {
        std::string text = "Failed to make object with '"+std::string(e.what())+"' for "+ obj.dump();
        FormatExcept("Failed to make object here ",e.what(), " for ",obj.dump());
        return Layout::Make<ErrorElement>(attr::Str{text}, Loc{layout.pos}, Size{layout.size});
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
                            std::string ret;
                            if(variable.is<std::string>()) {
                                ret = variable.value<std::string>(modifiers.sub);
                            } else if(variable.is<file::Path>()) {
                                ret = variable.value<file::Path>(modifiers.sub).str();
                            } else
                                ret = variable.value_string(modifiers.sub);
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

tl::expected<std::tuple<DefaultSettings, nlohmann::json>, const char*> load(const file::Path& path){
    static Timer timer;
    if(timer.elapsed() < 0.15) {
        return tl::unexpected("Have to wait longer to reload.");
    }
    timer.reset();
    
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        auto obj = nlohmann::json::parse(text);
        DefaultSettings defaults;
        try {
            if(obj.contains("defaults") && obj["defaults"].is_object()) {
                auto d = obj["defaults"];
                defaults.font = parse_font(d, defaults.font);
                if(d.contains("color")) defaults.textClr = parse_color(d["color"]);
                if(d.contains("fill")) defaults.fill = parse_color(d["fill"]);
                if(d.contains("line")) defaults.line = parse_color(d["line"]);
                if(d.contains("highlight_clr")) defaults.highlightClr = parse_color(d["highlight_clr"]);
                if(d.contains("window_color")) defaults.window_color = parse_color(d["window_color"]);
                if(d.contains("margins"))
                    defaults.margins = Meta::fromStr<Bounds>(d["margins"].dump());
            }
        } catch(const std::exception& ex) {
            FormatExcept("Cannot parse layout due to: ", ex.what());
            return tl::unexpected(ex.what());
        }
        return std::make_tuple(defaults, obj["objects"]);
    } else
        return tl::unexpected("Nothing changed.");
}

void DynamicGUI::update_objects(DrawStructure& g, const Layout::Ptr& o, const Context& context, State& state) {
    auto hash = (std::size_t)o->custom_data("object_index");
    if(not hash) {
        //! if this is a Layout type, need to iterate all children as well:
        if(o.is<Layout>()) {
            for(auto &child : o.to<Layout>()->objects()) {
                update_objects(g, child, context, state);
            }
        } else {
            //print("Object ", *o, " has no hash and is thus not updated.");
        }
        return;
    }
    //print("updating ", o->type()," with index ", hash, " for ", (uint64_t)o.get(), " ", o->name());
    
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
                    obj.state = std::make_unique<State>();
                    Context tmp = context;
                    for(auto &v : vector) {
                        tmp.variables["i"] = v;
                        auto ptr = parse_object(obj.child, tmp, *obj.state, context.defaults);
                        update_objects(g, ptr, tmp, *obj.state);
                        ptrs.push_back(ptr);
                        //print("Creating i", i++," to ", tmp.variables["i"]->value_string("pos"), " for ",ptr->pos(), " with hash ", hash);
                        //o.to<Layout>()->parent()->stage()->print(nullptr);
                    }
                    
                    o.to<Layout>()->set_children(ptrs);
                    
                } else {
                    Context tmp = context;
                    for(size_t i=0; i<obj.cache.size(); ++i) {
                        tmp.variables["i"] = obj.cache[i];
                        //print("Setting i", i," to ", tmp.variables["i"]->value_string("pos"), " for ",o.to<Layout>()->children()[i]->pos(), " with hash ", hash);
                        //o.to<Layout>()->children()[i]->set_pos(Meta::fromStr<Vec2>(tmp.variables["i"]->value_string("pos")));
                        auto p = o.to<Layout>()->children().at(i);
                        if(p)
                            update_objects(g, p, tmp, *obj.state);
                        //p->parent()->stage()->print(nullptr);
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
        //print("Found pattern ", pattern, " at index ", hash);
        
        if(it->second.contains("fill")) {
            try {
                auto fill = resolve_variable_type<Color>(pattern.at("fill"), context);
                LabeledField::delegate_to_proper_type(FillClr{fill}, o);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("color")) {
            try {
                auto clr = resolve_variable_type<Color>(pattern.at("color"), context);
                LabeledField::delegate_to_proper_type(TextClr{clr}, o);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        if(it->second.contains("line")) {
            try {
                auto line = resolve_variable_type<Color>(pattern.at("line"), context);
                LabeledField::delegate_to_proper_type(LineClr{line}, o);
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        
        if(pattern.contains("pos")) {
            try {
                o->set_pos(resolve_variable_type<Vec2>(pattern.at("pos"), context));
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("scale")) {
            try {
                o->set_scale(resolve_variable_type<Vec2>(pattern.at("scale"), context));
                //print("Setting pos of ", *o, " to ", pos, " (", o->parent(), " hash=",hash,") with ", o->name());
                
            } catch(const std::exception& e) {
                FormatError("Error parsing context; ", pattern, ": ", e.what());
            }
        }
        
        if(pattern.contains("size")) {
            try {
                auto size = pattern.at("size");
                if(size == "auto")
                {
                    if(o.is<Layout>())
                        o.to<Layout>()->auto_size();
                    else
                        FormatExcept("pattern for size should only be auto for layouts, not: ", *o);
                } else {
                    o->set_size(resolve_variable_type<Size2>(size, context));
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
                //print("pattern executed for ", (uint64_t)o.get(), " -> ", output, " hash=", hash);
                text->set_txt(output);
                text->set_name(output);
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
    
    //! list contents loops below
    if(auto it = state.lists.find(hash); it != state.lists.end()) {
        ListContents &obj = it->second;
        if(context.variables.contains(obj.variable)) {
            if(context.variables.at(obj.variable)->is<std::vector<std::shared_ptr<VarBase_t>>&>()) {
                
                auto& vector = context.variables.at(obj.variable)->value<std::vector<std::shared_ptr<VarBase_t>>&>("");
                
                IndexScopeHandler handler{state._current_index};
                //if(vector != obj.cache) {
                    std::vector<DetailItem> ptrs;
                    //obj.cache = vector;
                    obj.state = std::make_unique<State>();
                    Context tmp = context;
                
                size_t index=0;
                auto convert_to_item = [&gc = context, &index, &obj, &o](sprite::Map&, const nlohmann::json& item_template, Context& context) -> DetailItem
                {
                    DetailItem item;
                    if(item_template.contains("text") && item_template["text"].is_string()) {
                        item.set_name(parse_text(item_template["text"].get<std::string>(), context));
                    }
                    if(item_template.contains("detail") && item_template["detail"].is_string()) {
                        item.set_detail(parse_text(item_template["detail"].get<std::string>(), context));
                    }
                    if(item_template.contains("action") && item_template["action"].is_string()) {
                        auto action = parse_text(item_template["action"].get<std::string>(), context);
                        
                        if(not obj.on_select_actions.contains(index)
                           || std::get<0>(obj.on_select_actions.at(index)) != action)
                        {
                            obj.on_select_actions[index] = std::make_tuple(
                                index, [&gc = gc, ptr = o.get(), index = index, action = action](){
                                    print("Clicked item at ", index, " with action ", action);
                                    if(gc.actions.contains(action)) {
                                        gc.actions.at(action)(Meta::toStr(index));
                                    }
                                }
                            );
                        }
                    }
                    return item;
                };
                
                    for(auto &v : vector) {
                        tmp.variables["i"] = v;
                        try {
                            auto &ref = v->value<sprite::Map&>("");
                            auto item = convert_to_item(ref, obj.item, tmp);
                            ptrs.emplace_back(std::move(item));
                            ++index;
                        } catch(const std::exception& ex) {
                            FormatExcept("Cannot create list items for template: ", obj.item.dump(), " and type ", v->class_name());
                        }
                    }
                    
                    o.to<ScrollableList<DetailItem>>()->set_items(ptrs);
                    
                /*} else {
                    Context tmp = context;
                    for(size_t i=0; i<obj.cache.size(); ++i) {
                        tmp.variables["i"] = obj.cache[i];
                        //print("Setting i", i," to ", tmp.variables["i"]->value_string("pos"), " for ",o.to<Layout>()->children()[i]->pos(), " with hash ", hash);
                        //o.to<Layout>()->children()[i]->set_pos(Meta::fromStr<Vec2>(tmp.variables["i"]->value_string("pos")));
                        auto p = o.to<Layout>()->children().at(i);
                        if(p)
                            update_objects(g, p, tmp, *obj.state);
                        //p->parent()->stage()->print(nullptr);
                    }
                }*/
            }
        }
        return;
    }
}

void DynamicGUI::update(Layout* parent, const std::function<void(std::vector<Layout::Ptr>&)>& before_add) {
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
            
            auto [defaults, layout] = result.value();
            context.defaults = defaults;
            
            std::vector<Layout::Ptr> objs;
            State tmp;
            for(auto &obj : layout) {
                auto ptr = parse_object(obj, context, tmp, context.defaults);
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
    
    if(context.defaults.window_color != Transparent) {
        if(base)
            base->set_background_color(context.defaults.window_color);
        else {
            static std::once_flag flag;
            std::call_once(flag, [&](){
                FormatWarning("Cannot set background color to ", context.defaults.window_color);
            });
        }
    }
    
    if(parent) {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            parent->set_children(copy);
        } else {
            parent->set_children(objects);
        }
        
        for(auto &obj : objects) {
            update_objects(*graph, obj, context, state);
        }
        
    } else {
        //! check if we need anything more to be done to the objects before adding
        if(before_add) {
            auto copy = objects;
            before_add(copy);
            for(auto &obj : copy) {
                update_objects(*graph, obj, context, state);
                graph->wrap_object(*obj);
            }
            
        } else {
            for(auto &obj : objects) {
                update_objects(*graph, obj, context, state);
                graph->wrap_object(*obj);
            }
        }
    }
    
    dyn::update_tooltips(*graph, state);
}

DynamicGUI::operator bool() const {
    return graph != nullptr;
}

void DynamicGUI::clear() {
    graph = nullptr;
    objects.clear();
    state = {};
}

void update_tooltips(DrawStructure& graph, State& state) {
    if(not state._settings_tooltip) {
        state._settings_tooltip = Layout::Make<SettingsTooltip>();
    }
    
    Layout::Ptr found = nullptr;
    std::string name;
    std::unique_ptr<sprite::Reference> ref;
    
    for(auto & [key, ptr] : state._text_fields) {
        if(not ptr)
            continue;
        
        ptr->_text->set_clickable(true);
        
        if(ptr->representative()->hovered() && (ptr->representative().ptr.get() == graph.hovered_object() || dynamic_cast<Textfield*>(graph.hovered_object()))) {
            found = ptr->representative();
            name = ptr->_ref.get().name();
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
