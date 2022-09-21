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
        std::vector<std::shared_ptr<Text>> texts;
        std::vector<Vec2> positions;
        Vec2 _org_position;
        
        struct Settings {
            Vec2 max_size{-1, -1};
            Margins margins{5, 5};
            Color text_color = White;
            Font default_font = Font(0.75);
            std::string txt;
            Alpha alpha{1};
            
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
            set_clickable(true);
        }
        
        const auto& text() const { return _settings.txt; }
        const auto& max_size() const { return _settings.max_size; }
        
    private:
        using Drawable::set;
        void set(const std::string& str) { set_txt(str); }
        void set(TextClr clr) { set_text_color(clr); }
        void set(SizeLimit limit) { set_max_size(limit); }
        void set(Margins margins) { set_margins(margins); }
        void set(Alpha alpha) { set_alpha(alpha); }
        void set(Font font) { set_default_font(font); }
        
    public:
        virtual ~StaticText() {
            texts.clear();
        }
        
        void set_txt(const std::string& txt);
        
        void set_text_color(const Color& c) {
            if(c == _settings.text_color)
                return;
            
            _settings.text_color = c;
            update_text();
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
    };
}

#endif
