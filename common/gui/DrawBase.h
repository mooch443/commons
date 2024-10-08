#ifndef _DRAW_BASE_H
#define _DRAW_BASE_H

#include <commons.pc.h>
#include <gui/types/Basic.h>
#include <gui/Event.h>
//#include <gui/GuiTypes.h>
//#include <gui/DrawStructure.h>

namespace cmn::gui {
    typedef uint32_t uint;
    class Drawable;
    class Text;
    class DrawStructure;

    enum class LoopStatus {
        IDLE,
        UPDATED,
        END
    };
    
    class Base {
    protected:
        GETTER(bool, frame_recording);
        std::function<Bounds(const std::string&, Drawable*, const Font&)> _previous_line_bounds;
        std::function<Float2_t(const Font&)> _previous_line_spacing;
        Base *_previous_base;
        
    public:
        Base();
        virtual ~Base();
        
        virtual LoopStatus update_loop() { return LoopStatus::IDLE; }
        virtual void set_background_color(const Color&) {}
        
        virtual void set_frame_recording(bool v) {
            _frame_recording = v;
        }
        
        virtual void paint(DrawStructure& s) = 0;
        virtual void set_title(std::string) = 0;
        virtual void set_window_size(Size2) = 0;
        virtual void set_window_bounds(Bounds) = 0;
        virtual Bounds get_window_bounds() const = 0;
        virtual const std::string& title() const = 0;
        virtual Size2 window_dimensions() const;
        virtual Event toggle_fullscreen(DrawStructure&g);
        
        virtual Float2_t text_width(const Text &text) const;
        virtual Float2_t text_height(const Text &text) const;
        
        static Size2 text_dimensions(const std::string& text, Drawable* obj = NULL, const Font& font = {});
        
        virtual Bounds text_bounds(const std::string& text, Drawable*, const Font& font);
        static Bounds default_text_bounds(const std::string& text, Drawable* obj = NULL, const Font& font = {});
        static void set_default_text_bounds(std::function<Bounds(const std::string&, Drawable*, const Font&)>);
        
        virtual Float2_t line_spacing(const Font& font);
        
        static Float2_t default_line_spacing(const Font& font);
        static void set_default_line_spacing(std::function<Float2_t(const Font&)>);
    };
}

#endif
