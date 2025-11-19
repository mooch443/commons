#include "CornerFlags.h"

namespace cmn::gui {

std::string CornerFlags::toStr() const
{
    const std::string radiusStr = Meta::toStr(radius);

    // All corners → just “[radius]”
    if (mask == All)
        return "[" + radiusStr + "]";

    // Compose token list
    std::vector<std::string> toks;
    auto add = [&](const char* t){ toks.emplace_back(std::string("'")+t+"'"); };

    if (mask == (TopLeft | BottomLeft))             add("left");
    else if (mask == (TopRight | BottomRight))      add("right");
    else if (mask == (TopLeft | TopRight))          add("top");
    else if (mask == (BottomLeft | BottomRight))    add("bottom");
    else {
        if (top_left())      add("tl");
        if (top_right())     add("tr");
        if (bottom_right())  add("br");
        if (bottom_left())   add("bl");
    }

    std::string out = "[";
    for (size_t i = 0; i < toks.size(); ++i) {
        if (i) out += ',';
        out += toks[i];
    }
    if (!toks.empty()) out += ',';
    out += radiusStr;
    out += ']';
    return out;
}


glz::json_t CornerFlags::to_json() const
{
    using namespace glz;

    // Shortcut: all corners rounded → single‑element array with radius
    if (mask == All) {
        return radius;
    }

    // Build array of tokens + radius
    std::vector<json_t> arr;

    const auto add = [&](const char* t){ arr.emplace_back(std::string(t)); };

    if (mask == (TopLeft | BottomLeft))          add("left");
    else if (mask == (TopRight | BottomRight))   add("right");
    else if (mask == (TopLeft | TopRight))       add("top");
    else if (mask == (BottomLeft | BottomRight)) add("bottom");
    else {
        if (top_left())      add("tl");
        if (top_right())     add("tr");
        if (bottom_right())  add("br");
        if (bottom_left())   add("bl");
    }

    arr.emplace_back(radius);
    return arr;
}

}
