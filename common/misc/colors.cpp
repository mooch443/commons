#include "colors.h"


namespace gui {

Color Color::fromStr(const std::string& str) {
    auto s = utils::lowercase(str);
    if (s == "red") return gui::Red;
    if (s == "blue") return gui::Blue;
    if (s == "green") return gui::Green;
    if (s == "yellow") return gui::Yellow;
    if (s == "cyan") return gui::Cyan;
    if (s == "white") return gui::White;
    if (s == "black") return gui::Black;
    if (s == "gray") return gui::Gray;
    if (s == "darkgray") return gui::DarkGray;
    if (s == "lightgray") return gui::LightGray;
    if (s == "transparent") return gui::Transparent;
    if (s == "purple") return gui::Purple;

    auto vec = Meta::fromStr<std::vector<uchar>>(str);
    if (vec.empty())
        return Color();
    if (vec.size() != 4 && vec.size() != 3)
        throw CustomException(type<std::invalid_argument>, "Can only initialize Color with three or four elements. (", str, ")");
    return Color(vec[0], vec[1], vec[2], vec.size() == 4 ? vec[3] : 255);
}

}

