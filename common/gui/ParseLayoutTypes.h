#pragma once
#include <commons.pc.h>
#include <gui/types/Layout.h>
#include <nlohmann/json.hpp>
#include <gui/DynamicGUI.h>

namespace gui::dyn {

template<typename T>
auto get(State& state, const nlohmann::json& obj, T de, auto name, uint64_t hash) {
    if(obj.count(name)) {
        //if constexpr(std::same_as<T, Color>)
        {
            // is it a color variable?
            if(obj[name].is_string()) {
                state.patterns[hash][name] = obj[name].template get<std::string>();
                //print("pattern for ", name, " at object ", obj[name].template get<std::string>(), " at ", hash);
                //std::cout << obj << std::endl;
                return de;
            }
        }
        return Meta::fromStr<T>(obj[name].dump());
    }
    return de;
}

class LayoutContext {
public:
    const nlohmann::json& obj;
    State& state;
    
    Vec2 scale;
    Size2 size;
    Vec2 pos;
    Vec2 origin;
    Bounds margins;
    std::string name;
    Color fill;
    Color line;
    bool clickable;
    Font font;
    uint64_t hash;
    LayoutType::Class type;
    
    auto get(auto de, auto name) {
        return gui::dyn::get(state, obj, de, name, hash);
    }
    
    // Initialize from a JSON object
    LayoutContext(const nlohmann::json& obj, State& state);
    
    template <LayoutType::Class T>
    Layout::Ptr create_object(const Context&) {
        static_assert("Template not specialized for this LayoutType");
        return nullptr;
    }
    
    // Specializations
    template <>
    Layout::Ptr create_object<LayoutType::combobox>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::image>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::vlayout>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::hlayout>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::gridlayout>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::collection>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::settings>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::button>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::checkbox>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::textfield>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::stext>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::rect>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::text>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::circle>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::each>(const Context&);

    template <>
    Layout::Ptr create_object<LayoutType::condition>(const Context&);
    
    void finalize(const Layout::Ptr& ptr);
};

Image::Ptr load_image(const file::Path& path);

}
