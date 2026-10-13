#pragma once
#include <commons.pc.h>
#include <gui/types/Entangled.h>
#include <gui/types/Layout.h>

namespace cmn::gui {

class StaticText;

class ErrorElement : public Entangled {
public:
    struct Settings {
        Bounds bounds = Bounds(0, 0, 100, 33);
        Color fill_clr = Red.alpha(50);
        Color line_clr = Red.exposureHSL(0.5).alpha(50);
        Color text_clr = White;
        Font font = Font(0.35, Align::Left);
        std::string content;
        SizeLimit size_limit = SizeLimit(150,0);
    };
    
protected:
    Settings _settings;
    std::shared_ptr<StaticText> _text;
    Layout::Ptr _failed_object;
    
public:
    template<typename... Args>
    ErrorElement(Args... args)
    {
        create(std::forward<Args>(args)...);
    }
    
    ~ErrorElement();
    
    template<typename... Args>
    void create(Args... args) {
        if constexpr(not contains_type<attr::Str, Args...>())
            set(attr::Str{Settings{}.content});
        if constexpr(not contains_type<attr::Font, Args...>())
            set(Settings{}.font);
        if constexpr(not contains_type<attr::FillClr, Args...>())
            set(attr::FillClr{Settings{}.fill_clr});
        if constexpr(not contains_type<attr::LineClr, Args...>())
            set(attr::LineClr{Settings{}.line_clr});
        if constexpr(not contains_type<attr::TextClr, Args...>())
            set(attr::TextClr{Settings{}.text_clr});
        if constexpr(not contains_type<attr::SizeLimit, Args...>())
            set(Settings{}.size_limit);

        (set(std::forward<Args>(args)), ...);
        init();
    }
    
public:
    using Entangled::set;

    const Layout::Ptr& failed_object() const { return _failed_object; }
    void set_failed_object(Layout::Ptr object) { _failed_object = std::move(object); }
    
    void set(attr::Font font);
    void set(attr::FillClr clr) override;
    void set(attr::LineClr clr) override;
    void set(attr::TextClr clr);
    void set(attr::Str content);
    void set(attr::SizeLimit limit);
    
    void set_bounds(const Bounds& bds) override;
    void set_pos(const Vec2& p) override;
    void set_size(const Size2& p) override;
    
protected:
    void init();
    void update() override;
};

}
