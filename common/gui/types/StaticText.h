#ifndef _STATIC_TEXT_H
#define _STATIC_TEXT_H

#include <gui/types/Drawable.h>
#include <gui/types/Basic.h>
#include <gui/GuiTypes.h>
#include <gui/DrawStructure.h>

namespace gui {

    struct TRange {
        Range<size_t> range;
        Font font;
        std::string name, text;
        std::set<TRange> subranges;
        size_t after;
        size_t before;
        Color color;
        
        TRange(std::string n = "", size_t i = 0, size_t before = 0);
        
        void close(size_t i, const std::string& text, size_t after);
        
        bool operator<(const TRange& other) const;
        
        std::string toStr() const;
            
        static std::string class_name();
    };

    class StaticText : public Entangled {
    public:
        NUMBER_ALIAS(FadeOut_t, float)
        NUMBER_ALIAS(Shadow_t, float)
        
    private:
        std::vector<std::shared_ptr<Text>> texts;
        std::vector<Vec2> positions;
        Vec2 _org_position;
        derived_ptr<ExternalImage> _fade_out;
        
        struct Settings {
            Vec2 max_size{-1, -1};
            Margins margins{5, 5, 5, 5};
            Color text_color = White;
            Color line_color = Transparent;
            Font default_font = Font(0.75);
            std::string txt;
            Alpha alpha{1};
            Alpha fill_alpha{0};
            FadeOut_t fade_out{0.f};
            Shadow_t shadow{0.f};
            
        } _settings;
        
    public:
        struct RichString {
            std::string str, parsed;
            Font font;
            Vec2 pos;
            Color clr;
            
            RichString(const std::string& str = "", const Font& font = Font(), const Vec2& pos = Vec2(), const Color& clr = Color());
            
            static std::string parse(const std::string& txt);
            
            void convert(std::shared_ptr<Text> text) const;
        };
        
    public:
        template<typename... Args>
        StaticText(Args... args) {
            create(std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void create(Args... args) {
            (set(std::forward<Args>(args)), ...);
        }
        
        const auto& text() const { return _settings.txt; }
        const auto& max_size() const { return _settings.max_size; }
        
    public:
        using Entangled::set;
        void set(const Str& str) { set_txt(str); }
        void set(TextClr clr) { set_text_color(clr); }
        void set(LineClr clr) override { set_line_color(clr); }
        void set(Font font) { set_default_font(font); }
        void set(SizeLimit limit) { set_max_size(limit); }
        void set(Margins margins) { set_margins(margins); }
        void set(Alpha alpha) { set_alpha(alpha); }
        void set(FadeOut_t fade_out) {
            if(_settings.fade_out == fade_out)
                return;
            _settings.fade_out = fade_out;
            set_content_changed(true);
        }
        void set(Shadow_t shadow) {
            if(_settings.shadow == shadow)
                return;
            _settings.shadow = shadow;
            set_content_changed(true);
        }
        
        void set_bounds(const Bounds& bounds) override {
            if(not this->bounds().Equals(bounds)) {
                Entangled::set_bounds(bounds);
                set_content_changed(true);
            }
        }
        
    public:
        virtual ~StaticText() {
            texts.clear();
            _fade_out = nullptr;
        }
        
        using Entangled::set_background;
        void set_background(const Color& color, const Color& line) override;
        virtual std::string toStr() const override {
            return std::string(type().name()) + " " + Meta::toStr(_settings.txt);
        }
        
        void set_txt(const std::string& txt);
        
        void set_text_color(const Color& c) {
            if(c == _settings.text_color)
                return;
            
            _settings.text_color = c;
            update_text();
        }
        
        void set_line_color(const Color& c) {
            if(c == _settings.line_color)
                return;
            
            _settings.line_color = c;
            set_background(Transparent, _settings.line_color);
        }
        
        void set_alpha(const Alpha& c) {
            if(c == _settings.alpha)
                return;
            
            _settings.alpha = c;
            set_content_changed(true);
        }
        
        void set_margins(const Margins& margin);
        
        void update() override;
        void add_string(std::shared_ptr<RichString> ptr, std::vector<std::shared_ptr<RichString>>& output, Vec2& offset);
        void structure_changed(bool downwards) override;
        virtual void set_size(const Size2& size) override;
        
        virtual Size2 size() override;
        virtual const Bounds& bounds() override;
        void set_max_size(const Size2&);
        void set_default_font(const Font&);
        
        static std::vector<TRange> to_tranges(const std::string& _txt);
        
    private:
        void update_text();
        void add_shadow();
    };
}

#endif
