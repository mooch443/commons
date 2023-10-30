#pragma once
#include <commons.pc.h>
#include <gui/types/Layout.h>
#include <nlohmann/json.hpp>
#include <gui/DynamicGUI.h>

namespace gui::dyn {

Font parse_font(const nlohmann::json& obj, Font font = Font(0.75));

inline Color parse_color(const auto& obj) {
    if(not obj.is_array())
        throw std::invalid_argument("Color is not an array.");
    if(obj.size() < 3)
        throw std::invalid_argument("Color is not an array of RGB.");
    uint8_t a = 255;
    if(obj.size() >= 4) {
        a = obj[3].template get<uint8_t>();
    }
    return Color(obj[0].template get<uint8_t>(),
                 obj[1].template get<uint8_t>(),
                 obj[2].template get<uint8_t>(),
                 a);
}

template<typename T, bool assume_exists = false>
auto get(State& state, const nlohmann::json& obj, T de, auto name, uint64_t hash) 
{
    auto it = obj.find(name);
    
    if constexpr(assume_exists) {
        if(it == obj.end())
            throw InvalidArgumentException("Expected property ", name, " in ", obj.dump());
    } else {
        if(it == obj.end())
            return de;
    }
    
    auto& o = *it;
    if constexpr(not are_the_same<std::string, T>
                 && not are_the_same<file::Path, T>)
    {
        if(o.is_string()) {
            state.patterns[hash][name] = Pattern{o.template get<std::string>()};
            return de;
        }
        return Meta::fromStr<T>(o.dump());
        
    } else if constexpr(are_the_same<file::Path, T>) {
        return o.template get<file::Path>();
    } else {
        auto val = o.template get<std::string>();
        if(utils::contains(val, '{')) {
            state.patterns[hash][name] = Pattern{val};
            return de;
        }
        return val;
    }
}

class LayoutContext {
public:
    const nlohmann::json& obj;
    State& state;
    
    DefaultSettings _defaults;
    
    Vec2 scale;
    Size2 size, max_size;
    Vec2 pos;
    Vec2 origin;
    Bounds pad;
    std::string name;
    Color fill;
    Color line;
    Color highlight_clr;
    Color vertical_clr;
    Color horizontal_clr;
    TextClr textClr;
    bool clickable;
    Font font;
    uint64_t hash;
    LayoutType::Class type;
    
    auto get(auto de, auto name) {
        return gui::dyn::get(state, obj, de, name, hash);
    }
    
    // Initialize from a JSON object
    LayoutContext(const nlohmann::json& obj, State& state, DefaultSettings defaults, uint64_t hash = 0);
    
    template <LayoutType::Class T>
    Layout::Ptr create_object(const Context&) {
        static_assert("Template not specialized for this LayoutType");
        return nullptr;
    }
    
    
    void finalize(const Layout::Ptr& ptr);
};

Image::Ptr load_image(const file::Path& path);


// Specializations
template <>
Layout::Ptr LayoutContext::create_object<LayoutType::combobox>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::image>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::vlayout>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::hlayout>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::gridlayout>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::collection>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::settings>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::button>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::checkbox>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::textfield>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::stext>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::rect>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::text>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::circle>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::each>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::condition>(const Context&);

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::list>(const Context&);

}
