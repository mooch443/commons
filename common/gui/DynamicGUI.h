#pragma once

#include <commons.pc.h>
#include <JSON.h>
#include <gui/colors.h>
#include <file/Path.h>
#include <gui/types/Layout.h>

namespace gui {
namespace dyn {

ENUM_CLASS(LayoutType, vlayout, hlayout, button, text, rect);

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

struct Context {
    std::unordered_map<std::string, std::function<void(Event)>> actions;
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> variables;
    std::unordered_map<std::string, std::function<Color()>> color_variables;
};

struct State {
    size_t object_index{0};
    std::unordered_map<size_t, std::string> patterns;
};

Layout::Ptr parse_object(const nlohmann::json& obj,
                         const Context& context,
                         State& state);

std::string parse_text(const std::string& pattern, const Context& context);

void update_objects(const Layout::Ptr& o, const Context& context, const State& state);

inline auto load(const file::Path& path){
    auto text = path.read_file();
    static std::string previous;
    if(previous != text) {
        previous = text;
        return nlohmann::json::parse(text)["objects"];
    } else
        throw std::invalid_argument("Nothing changed.");
}

void update_layout(const file::Path&, Context&, State&, std::vector<Layout::Ptr>&);

}
}
