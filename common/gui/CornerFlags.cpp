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
    auto add = [&](const char* t){ toks.emplace_back(t); };

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

CornerFlags CornerFlags::fromStr(const std::string& s)
{
    std::string_view sv = utils::trim(std::string_view(s));

    // Bare radius (no brackets) → all corners
    if (!sv.empty() && sv.front() != '[') {
        float r = Meta::fromStr<float>(std::string(sv));
        return CornerFlags(All, r);
    }

    // Remove leading ‘[’ and trailing ‘]’
    if (!sv.empty() && sv.front() == '[')  sv.remove_prefix(1);
    if (!sv.empty() && sv.back()  == ']')  sv.remove_suffix(1);
    sv = utils::trim(sv);

    if (sv.empty())
        return CornerFlags();

    // Split by commas
    float radius = CornerFlags::DefaultRadius;
    auto parts = util::parse_array_parts(sv);
    
    // search radius
    for(auto &part : parts) {
        if(not utils::beginsWith(part, '"')
           && not utils::beginsWith(part, '\''))
        {
            radius = Meta::fromStr<float>(part);
            break;
        }
    }
    
    uint8_t m = None;
    auto addCorner = [&](uint8_t c){ m |= c; };

    for (auto& raw : parts) {
        // Trim whitespace and strip optional single or double quotes
        std::string tok = utils::trim(raw);
        if (!tok.empty() && (tok.front() == '\"' || tok.front() == '\'')) {
            tok.erase(tok.begin());
        }
        if (!tok.empty() && (tok.back() == '\"' || tok.back() == '\'')) {
            tok.pop_back();
        }
        // Convert to lower‑case for uniform comparison
        std::transform(tok.begin(), tok.end(), tok.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (tok == "all")
            { m = All; break; }
        else if (tok == "none")
            { m = None; }
        else if (tok == "left")
            { addCorner(TopLeft  | BottomLeft); }
        else if (tok == "right")
            { addCorner(TopRight | BottomRight); }
        else if (tok == "top")
            { addCorner(TopLeft  | TopRight); }
        else if (tok == "bottom")
            { addCorner(BottomLeft | BottomRight); }
        else if (tok == "tl" || tok == "topleft")
            { addCorner(TopLeft); }
        else if (tok == "tr" || tok == "topright")
            { addCorner(TopRight); }
        else if (tok == "br" || tok == "bottomright")
            { addCorner(BottomRight); }
        else if (tok == "bl" || tok == "bottomleft")
            { addCorner(BottomLeft); }
        // Unknown tokens are ignored.
    }

    if (m == None && parts.empty())
        m = All;   // [2.5] case handled here

    return CornerFlags(m, radius);
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
