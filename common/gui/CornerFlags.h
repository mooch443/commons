#pragma once
#include <commons.pc.h>

namespace cmn::gui {
struct CornerFlags {
    /// Bit‑mask for which rectangle corners are rounded
    enum Corner : uint8_t {
        TopLeft      = 1 << 0,
        TopRight     = 1 << 1,
        BottomRight  = 1 << 2,
        BottomLeft   = 1 << 3,
        All          = TopLeft | TopRight | BottomRight | BottomLeft,
        None         = 0
    };
    
    static constexpr float DefaultRadius = 10.0f;
    static constexpr Corner DefaultCorners = Corner::All;
    
    uint8_t mask = (uint8_t)Corner::All;   ///< bit‑mask specifying rounded corners
    float   radius = DefaultRadius; ///< common rounding radius
    
    /// constexpr constructor from mask & radius
    constexpr CornerFlags(uint8_t m = DefaultCorners, float r = DefaultRadius) : mask(m), radius(r) {}
    
    /// constexpr constructor from individual booleans
    constexpr CornerFlags(bool tl, bool tr, bool br, bool bl, float r = DefaultRadius)
    : mask( (tl ? TopLeft : 0) |
           (tr ? TopRight : 0) |
           (br ? BottomRight : 0) |
           (bl ? BottomLeft : 0) ),
    radius(r) {}
    
    /// helpers to query individual corners
    constexpr bool is_rounded(Corner c) const { return (mask & c) != 0; }
    constexpr bool top_left()     const { return is_rounded(TopLeft);     }
    constexpr bool top_right()    const { return is_rounded(TopRight);    }
    constexpr bool bottom_right() const { return is_rounded(BottomRight); }
    constexpr bool bottom_left()  const { return is_rounded(BottomLeft);  }
    
    /// set / clear a corner at run‑time
    constexpr void set(Corner c, bool v = true)
    {
        if(v) mask |= c;
        else  mask &= static_cast<uint8_t>(~c);
    }
    
    /// handy factories
    constexpr static CornerFlags Rounded(float r) { return CornerFlags(DefaultCorners, r); }
    constexpr static CornerFlags Square()         { return CornerFlags(DefaultCorners, DefaultRadius); }
    
    /// equality / inequality (constexpr)
    [[nodiscard]] constexpr bool operator==(const CornerFlags& other) const
    {
        return mask == other.mask && radius == other.radius;
    }
    [[nodiscard]] constexpr bool operator!=(const CornerFlags& other) const
    {
        return !(*this == other);
    }
    
    /// Convert this CornerFlags mask to Dear ImGui draw‑corner flags.
    /// Does not take the radius into account – only which corners are rounded.
    [[nodiscard]] constexpr ImDrawFlags to_imdraw_flags() const
    {
        ImDrawFlags f = 0;
        if (top_left())      f |= ImDrawFlags_RoundCornersTopLeft;
        if (top_right())     f |= ImDrawFlags_RoundCornersTopRight;
        if (bottom_right())  f |= ImDrawFlags_RoundCornersBottomRight;
        if (bottom_left())   f |= ImDrawFlags_RoundCornersBottomLeft;
        if (f == 0)          f = ImDrawFlags_RoundCornersNone;
        return f;
    }
    
    /// Explicit conversion operator – enables:
    ///     ImDrawFlags flags = static_cast<ImDrawFlags>(cornerFlags);
    constexpr explicit operator ImDrawFlags() const { return to_imdraw_flags(); }
    
    static std::string class_name() { return "CornerFlags"; }
    
    
    static CornerFlags fromStr(cmn::StringLike auto&& s)
    {
        std::string_view sv = utils::trim(std::string_view(s));

        // Bare radius (no brackets) → all corners
        if (!sv.empty() && sv.front() != '[') {
            float r = Meta::fromStr<float>(sv);
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
            auto tok = utils::trim(raw);
            if (!tok.empty()
                && (tok.front() == '\"'
                    || tok.front() == '\''))
            {
                //tok.erase(tok.begin());
                tok = tok.substr(1);
            }
            if (!tok.empty() && (tok.back() == '\"' || tok.back() == '\'')) {
                //tok.pop_back();
                tok = tok.substr(0, tok.length() - 1);
            }
            // Convert to lower‑case for uniform comparison
            /*std::transform(tok.begin(), tok.end(), tok.begin(),
                           [](unsigned char c){ return std::tolower(c); });*/

            if (cmn::utils::lowercase_equal_to(tok, "all"))
                { m = All; break; }
            else if (cmn::utils::lowercase_equal_to(tok, "none"))
                { m = None; }
            else if (cmn::utils::lowercase_equal_to(tok, "left"))
                { addCorner(TopLeft  | BottomLeft); }
            else if (cmn::utils::lowercase_equal_to(tok , "right"))
                { addCorner(TopRight | BottomRight); }
            else if (cmn::utils::lowercase_equal_to(tok, "top"))
                { addCorner(TopLeft  | TopRight); }
            else if (cmn::utils::lowercase_equal_to(tok, "bottom"))
                { addCorner(BottomLeft | BottomRight); }
            else if (cmn::utils::lowercase_equal_to(tok, "tl") || cmn::utils::lowercase_equal_to(tok, "topleft"))
                { addCorner(TopLeft); }
            else if (cmn::utils::lowercase_equal_to(tok, "tr") || cmn::utils::lowercase_equal_to(tok, "topright"))
                { addCorner(TopRight); }
            else if (cmn::utils::lowercase_equal_to(tok, "br") || cmn::utils::lowercase_equal_to(tok,"bottomright"))
                { addCorner(BottomRight); }
            else if (cmn::utils::lowercase_equal_to(tok, "bl") || cmn::utils::lowercase_equal_to(tok, "bottomleft"))
                { addCorner(BottomLeft); }
            // Unknown tokens are ignored.
        }

        if (m == None && parts.empty())
            m = All;   // [2.5] case handled here

        return CornerFlags(m, radius);
    }

    
    // ──────────────────────────────────────────────────────────────────────────────
    // CornerFlags string-serialisation helpers
    //
    // Human-readable, Python-list-like representation:
    //
    //   • All corners rounded:       “[2.5]”          (radius only)
    //     (a bare “2.5” without brackets also parses)
    //
    //   • Left corners rounded:      “[left,2.5]”
    //   • Individual corners:        “[tl,tr,2.5]”
    //
    // Recognised tokens (case-insensitive):
    //   all, none, left, right, top, bottom,
    //   tl|topleft, tr|topright, br|bottomright, bl|bottomleft
    // ──────────────────────────────────────────────────────────────────────────────
    std::string toStr() const;
    
    
    glz::json_t to_json() const;
};

}
