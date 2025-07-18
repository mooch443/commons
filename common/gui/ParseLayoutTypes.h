#pragma once
#include <commons.pc.h>
#include <gui/types/Layout.h>
#include <gui/DynamicGUI.h>

namespace cmn::gui::dyn {

Font parse_font(const glz::json_t::object_t& obj, Font font = Font(0.75), std::string_view name = "font");

struct NumberParser {
    template<typename T>
        requires (std::integral<T> || std::same_as<T, float>)
    static constexpr T get(const auto& obj) {
        return static_cast<T>(obj.get_number());
    }
};

inline Color parse_color(const auto& obj) {
    if(not obj.is_array())
        throw std::invalid_argument("Color is not an array.");
    if(obj.size() < 3)
        throw std::invalid_argument("Color is not an array of RGB.");
    uint8_t a = 255;
    if(obj.size() >= 4) {
        a = obj[3].template get<double>();
    }
    return Color(obj[0].template get<double>(),
                 obj[1].template get<double>(),
                 obj[2].template get<double>(),
                 a);
    return Color{};
}

template<typename T, bool assume_exists = false>
auto get(State& state, const glz::json_t::object_t& obj, T de, auto name, uint64_t hash, std::string name_prefix="")
{
    auto it = obj.find(name);
    
    if constexpr(assume_exists) {
        if(it == obj.end())
            throw InvalidArgumentException("Expected property ", name, " in ", glz::write_json(obj).value_or("null"));
    } else {
        if(it == obj.end())
            return de;
    }
    
    auto& o = it->second;
    if constexpr(not are_the_same<std::string, T>
                 && not are_the_same<file::Path, T>)
    {
        if(o.is_string()) {
            state.register_pattern(hash, name_prefix+name, Pattern::prepare(o.template get<std::string>())); //Pattern{o.template get<std::string>(), {}});
            return de;
        }
        return Meta::fromStr<T>(glz::write_json(o).value());
        
    } else if constexpr(are_the_same<file::Path, T>) {
        return o.template get<file::Path>();
    } else {
        auto val = o.template get<std::string>();
        if(utils::contains(val, '{')) {
            state.register_pattern(hash, name_prefix+name, Pattern::prepare(val)); //Pattern{val, {}});
            return de;
        }
        return val;
    }
}

class LayoutContext {
public:
    GUITaskQueue_t *gui{nullptr};
    glz::json_t::object_t obj;
    State& state;
    const Context& context;
    
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
    Color cellFillClr;
    Color cellLineClr;
    CornerFlags_t corners{};
    uint16_t cellFillInterval{1};
    Size2 minCellSize;
    TextClr textClr;
    bool clickable;
    Font font;
    uint64_t hash;
    LayoutType::Class type;
    ZIndex zindex;
    
    auto get(auto de, auto name, std::string name_prefix="") {
        return gui::dyn::get(state, obj, de, name, hash, name_prefix);
    }
    
    // Initialize from a JSON object
    LayoutContext(GUITaskQueue_t*, const glz::json_t::object_t& obj, State& state, const Context& context, DefaultSettings defaults, uint64_t hash = 0);
    
    template <LayoutType::Class T>
    Layout::Ptr create_object() {
        static_assert("Template not specialized for this LayoutType");
        return nullptr;
    }
    
    
    void finalize(const Layout::Ptr& ptr);
};

Image::Ptr load_image(const file::Path& path);


// Specializations
template <>
Layout::Ptr LayoutContext::create_object<LayoutType::combobox>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::image>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::vlayout>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::hlayout>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::gridlayout>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::collection>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::settings>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::button>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::checkbox>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::line>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::textfield>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::stext>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::rect>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::text>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::circle>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::each>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::condition>();

template <>
Layout::Ptr LayoutContext::create_object<LayoutType::list>();

}
