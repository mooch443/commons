#pragma once

#include <gui/GuiTypes.h>
#include <gui/types/Entangled.h>

namespace cmn::gui {
    class Triangle final : public Entangled {
        GETTER(Color, fill);
        GETTER(Color, line);
        GETTER(std::vector<Vertex>, points);
        
    public:
        Triangle(const Vec2& center, const Size2& size, Float2_t angle = 0, const Color& fill = White, const Color& line = Transparent);
        Triangle(const std::vector<Vertex>& vertices);
        
        _CHANGE_SETTER(fill)
        _CHANGE_SETTER(line)
        
        bool in_bounds(Float2_t x, Float2_t y) override;
        
    protected:
        bool swap_with(Drawable *d) override;
        //void prepare() override;
        static std::vector<Vertex> simple_triangle(const Color& color, const Size2& size);
    };
}
