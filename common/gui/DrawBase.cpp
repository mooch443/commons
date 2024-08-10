#include "DrawBase.h"
#include <gui/DrawStructure.h>
#include <gui/GuiTypes.h>

namespace cmn::gui {
    Base *_latest_base = nullptr;
    std::function<Bounds(const std::string&, Drawable*, const Font&)> _restore_line_bounds;
    std::function<Float2_t(const Font&)> _restore_line_spacing;

    Size2 Base::window_dimensions() const { return Size2(-1); }

    Event Base::toggle_fullscreen(DrawStructure&g) {
        Event e(WINDOW_RESIZED);
        e.size.width = g.width();
        e.size.height = g.height();
        return e;
    }

    Size2 Base::text_dimensions(const std::string& text, Drawable* obj, const Font& font) {
        auto size = default_text_bounds(text, obj, font);
        return Size2(size.pos() + size.size());
    }

    std::function<Float2_t(const Font&)>& line_spacing_fn(){
        static std::function<Float2_t(const Font&)> fn = [](const Font& font) -> Float2_t
        {
            return (25 * font.size);
        };
        return fn;
    }
    void Base::set_default_line_spacing(std::function<Float2_t(const Font&)> fn) {
        line_spacing_fn() = fn;
    }
    Float2_t Base::default_line_spacing(const Font &font) {
        return line_spacing_fn()(font);
    }

    auto& text_bounds_fn() {
        static std::function<Bounds(const std::string&, Drawable*, const Font&)> fn = [](const std::string& text, Drawable*, const Font& font) -> Bounds {
            return Bounds(0, 0, text.length() * 11.3_F * font.size, line_spacing_fn()(font));
        };
        return fn;
    }
    Bounds Base::default_text_bounds(const std::string &text, Drawable* obj, const Font& font) {
        return text_bounds_fn()(text, obj, font);
    }
    void Base::set_default_text_bounds(std::function<Bounds (const std::string &, Drawable *, const Font &)> fn) {
        text_bounds_fn() = fn;
    }

    Float2_t Base::line_spacing(const Font& font) {
        return narrow_cast<Float2_t>((25_F * font.size));
    }
    Float2_t Base::text_width(const Text &text) const {
        return text.txt().length() * 8.5_F * text.font().size;
    }
    Float2_t Base::text_height(const Text &text) const {
        return 18_F * text.font().size;
    }
    Bounds Base::text_bounds(const std::string& text, Drawable*, const Font& font) {
        return Bounds(0_F, 0_F, text.length() * 11.3_F * font.size, 26_F * font.size);
    }

    Base::Base() {
        _previous_base = _latest_base;
        _previous_line_spacing = line_spacing_fn();
        _previous_line_bounds = text_bounds_fn();
        
        _restore_line_bounds = _previous_line_bounds;
        _restore_line_spacing = _previous_line_spacing;
        
        _latest_base = this;
        
        set_default_line_spacing([this](const Font& font) -> Float2_t {
            return this->line_spacing(font);
        });
        set_default_text_bounds([this](const std::string & text, Drawable *obj, const Font &font) -> Bounds {
            return this->text_bounds(text, obj, font);
        });
    }
    Base::~Base() {
        if(_latest_base == this) {
            set_default_text_bounds(_previous_line_bounds);
            set_default_line_spacing(_previous_line_spacing);
            
            _restore_line_spacing = [](const Font& font) -> Float2_t
                {
                    return narrow_cast<uint32_t>((25_F * font.size));
                };
            _restore_line_bounds = [](const std::string& text, Drawable*, const Font& font) -> Bounds
                {
                    return Bounds(0_F, 0_F, text.length() * 11.3_F * font.size, line_spacing_fn()(font));
                };
            
            _latest_base = _previous_base;
            
        } else {
            _restore_line_spacing = _previous_line_spacing;
            _restore_line_bounds = _previous_line_bounds;
        }
    }
}
