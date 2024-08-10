#pragma once

#include <commons.pc.h>
#include <misc/colors.h>
#include <file/Path.h>
#include <gui/types/Layout.h>
#include <misc/PackLambda.h>
#include <misc/SpriteMap.h>
#include <misc/Timer.h>
#include <gui/GuiTypes.h>
//#include <gui/types/Textfield.h>
//#include <gui/types/Dropdown.h>
//#include <gui/types/Checkbox.h>
//#include <gui/types/List.h>
//#include <gui/types/ListItemTypes.h>
#include <gui/dyn/VarProps.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>

namespace cmn::gui {
namespace dyn {

class LabeledField;
struct Context;
struct State;
struct Action;

ENUM_CLASS(LayoutType,
           each,
           condition,
           vlayout,
           hlayout,
           gridlayout,
           collection,
           button,
           circle,
           text,
           stext,
           rect,
           textfield,
           checkbox,
           settings,
           combobox,
           image,
           list,
           unknown);

template<typename Fn>
std::pair<std::string, std::function<void(Action)>> ActionFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::function<void(Action)>(std::forward<Fn>(func)));
}

template<typename Fn, typename T = typename cmn::detail::return_type<Fn>::type>
std::pair<std::string, std::shared_ptr<VarBase_t>> VarFunc(const std::string& name, Fn&& func) {
    return std::make_pair(name, std::shared_ptr<VarBase_t>(new Variable(std::forward<Fn>(func))));
}

class LayoutContext;

namespace Modules {
struct Module {
    std::string _name;
    std::function<void(size_t, State&, const Layout::Ptr&)> _apply;
};

void add(Module&&);
void remove(const std::string& name);
Module* exists(const std::string& name);
}

Layout::Ptr parse_object(GUITaskQueue_t*,
                         const glz::json_t::object_t& obj,
                         const Context& context,
                         State& state,
                         const DefaultSettings& defaults,
                         uint64_t hash = 0);

tl::expected<std::tuple<DefaultSettings, glz::json_t>, std::string> load(const std::string& text);

//! A simple struct that manages properties like the path to where the dyn gui is loaded from,
//! the current context and state, and the current layout as well as which DrawStructure it is rendered to.
struct DynamicGUI {
    GUITaskQueue_t* gui{nullptr};
    file::Path path;
    //DrawStructure* graph{nullptr};
    Context context;
    State state;
    Base* base;
    std::vector<Layout::Ptr> objects;
    Timer last_load, last_update;
    bool first_load{true}, first_update{true};
    std::string previous;
    std::future<tl::expected<std::tuple<DefaultSettings, glz::json_t>, std::string>> read_file_future;
    std::shared_ptr<CurrentObjectHandler> current_object_handler = std::make_shared<CurrentObjectHandler>();
#ifndef NDEBUG
    Timer _debug_timer;
#endif
    
    void update(DrawStructure& graph, Layout* parent, const std::function<void(std::vector<Layout::Ptr>&)>& before_add = nullptr);
    
    operator bool() const;
    void clear();
    
private:
    void reload(DrawStructure& graph);
public:
    static bool update_objects(GUITaskQueue_t* gui, DrawStructure& g, Layout::Ptr& o, const Context& context, State& state);
private:
};

void update_tooltips(DrawStructure&, State&);

}
}
