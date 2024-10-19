#pragma once

#include <gui/GuiTypes.h>
#include <gui/types/Entangled.h>

namespace cmn::gui {
template<typename T>
class NumericTextfield;

template<typename T>
class MetaTextfield;

class StaticText;

ATTRIBUTE_ALIAS(ClearText_t, std::optional<std::string>);

class Textfield : public Entangled {
public:
    using CheckTextFn = std::function<bool(std::string& text, char inserted, size_t at)>;
    ATTRIBUTE_ALIAS(CheckText_t, CheckTextFn)
    
    using VoidFn = std::function<void()>;
    ATTRIBUTE_ALIAS(OnEnter_t, VoidFn)
    ATTRIBUTE_ALIAS(OnTextChanged_t, VoidFn)
    ATTRIBUTE_ALIAS(OnTab_t, VoidFn)
    
    struct ReadOnly {
        bool read_only;
        
        operator bool() const {
            return read_only;
        }
    };

private:
    long_t _cursor_position;
    Rect _cursor = Rect(Box(0,0,2,30));
    Rect _selection_rect = Rect(FillClr{DarkCyan.alpha(100)});
    StaticText *_placeholder{nullptr};
    std::unique_ptr<StaticText> _clear_button;
    
    size_t _text_offset{0};
    size_t _display_text_len{0};
    
    long_t _selection_start;
    lrange _selection{-1, -1};
    
    bool _valid{true};
    
protected:
    struct Settings {
        CheckText_t check_text = nullptr;
        OnEnter_t on_enter = nullptr;
        OnTab_t on_tab = nullptr;
        OnTextChanged_t on_text_changed = nullptr;
        
        std::string text;
        std::string postfix;
        Font font{0.75};
        Color text_color = Black;
        Color fill_color = White.alpha(210);
        Color line_color = White.alpha(50);
        
        ClearText_t clear_text;
        bool read_only{false};
    } _settings;
    
private:
    Text _text_display;
    
public:
    template<typename... Args>
    Textfield(Args... args) {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        _settings = {};
        
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
    void init();
    
    virtual ~Textfield();
    
public:
    using Entangled::set;
    void set(const Str& text) { set_text(text); }
    void set(const Postfix& postfix) { set_postfix(postfix); }
    void set(FillClr fill) override { set_fill_color(fill); }
    void set(LineClr line) override { set_line_color(line); }
    void set(TextClr clr) { set_text_color(clr); }
    void set(ReadOnly ro) { set_read_only(ro); }
    void set(Font font) { set_font(font); }
    void set(CheckText_t check) { _settings.check_text = check; }
    void set(OnEnter_t enter) { on_enter(enter); }
    void set(OnTab_t tab) { on_tab(tab); }
    void set(OnTextChanged_t change) { on_text_changed(change); }
    void set(Placeholder_t p) { set_placeholder(p); }
    void set(ClearText_t c);
    
public:
    void set_text_color(const Color& c);
    void set_fill_color(const Color& c);
    void set_line_color(const Color& c);
    void set_postfix(const std::string&);
    
    const auto& text() const { return _settings.text; }
    const auto& postfix() const { return _settings.postfix; }
    auto read_only() const { return _settings.read_only; }
    auto valid() const { return _valid; }
    const auto& text_color() const { return _settings.text_color; }
    const auto& fill_color() const { return _settings.fill_color; }
    const auto& line_color() const { return _settings.line_color; }
    const auto& font() const { return _settings.font; }
    
    void set_size(const Size2& size) override {
        if(not bounds().size().Equals(size)) {
            Entangled::set_size(attr::Size(size));
            set_content_changed(true);
        }
    }
    void set_bounds(const Bounds& bounds) override {
        if(not bounds.Equals(this->bounds())) {
            Entangled::set_bounds(bounds);
            set_content_changed(true);
        }
    }
    
    virtual std::string name() const override { return "Textfield"; }
    
    void set_placeholder(const std::string& text);
    void set_text(const std::string& text);
    void set_font(Font font);
    void set_read_only(bool ro) {
        if(_settings.read_only == ro)
            return;
        
        _settings.read_only = ro;
        set_content_changed(true);
    }
    
    virtual void set_filter(const CheckText_t& fn) {
        _settings.check_text = fn;
        if(_settings.check_text)
            _settings.check_text(_settings.text, 0, 0);
    }
    
    void on_enter(const OnEnter_t& fn) {
        _settings.on_enter = fn;
    }
    
    void on_tab(const OnTab_t& fn) {
        _settings.on_tab = fn;
    }
    
    void enter();
    
    void on_text_changed(const OnTextChanged_t& fn) {
        _settings.on_text_changed = fn;
    }
    
    template<typename T>
    operator const NumericTextfield<T>&() const {
        if(dynamic_cast<const NumericTextfield<T>*>(this)) {
            return *static_cast<const NumericTextfield<T>*>(this);
        }
        
        throw U_EXCEPTION("Cannot cast.");
    }
    
    template<typename T>
    operator const MetaTextfield<T>&() const {
        if(dynamic_cast<const MetaTextfield<T>*>(this)) {
            return *static_cast<const MetaTextfield<T>*>(this);
        }
        
        throw U_EXCEPTION("Cannot cast.");
    }
    
protected:
    std::tuple<bool, bool> system_alt() const;
    
    void set_valid(bool valid) {
        if(_valid == valid)
            return;
        
        _valid = valid;
        set_dirty();
    }
    
    virtual bool isTextValid(std::string& text, char inserted, size_t at) {
        if(!_settings.check_text)
            return true;
        return _settings.check_text(text, inserted, at);
    }
    virtual std::string correctText(const std::string& text) {return text;}
    
private:
    void update() override;
    void move_cursor(Float2_t mx);
    
    bool onEnter(Event e);
    bool onControlKey(Event e);
    
    bool swap_with(Drawable* d) override {
        if(d->type() == Type::ENTANGLED) {
            auto ptr = dynamic_cast<Textfield*>(d);
            if(!ptr)
                return false;
            
            if(ptr->text() != text()) {
                set_text(ptr->text());
                set_content_changed(true);
            }
            
            return true;
        }
        
        return false;
    }
};

template<typename T>
class NumericTextfield : public Textfield {
    arange<T> _limits;
    
public:
    NumericTextfield(const T& number, const Bounds& bounds, arange<T> limits = {0, 0})
        : Textfield(Str(Meta::toStr(number)), Box(bounds)), _limits(limits)
    { }
    
    T get_value() const {
        try {
            return Meta::fromStr<T>(text());
        } catch(const std::logic_error&) {}
        
        if(text() == "-")
            return T(-0);
        return T(0);
    }
    
protected:
    virtual void set_filter(const CheckText_t&) override {
        // do nothing
        throw U_EXCEPTION("This function is disabled for numeric textfields.");
    }
    
    virtual bool isTextValid(std::string& text, char inserted, size_t at) override
    {
        if(text.empty() || text == "-")
            return true;
        
        if(std::is_integral<T>::value && inserted == '.')
            return false;
        
        if(inserted == 8 // erased
           || (inserted == '.' && !utils::contains(this->text(), '.'))
           || (inserted == '-' && at == 0 && !utils::contains(this->text(), '-'))
           || irange('0', '9'+1).contains(inserted))
        {
            if(text != "-") {
                try {
                    T v = Meta::fromStr<T>(text);
                    if(_limits.first != _limits.last && !_limits.contains(v)) {
                        if(v < _limits.first)
                            v = _limits.first;
                        else if(v > _limits.last)
                            v = _limits.last;
                        else
                            return false;
                        
                        // correct value
                        text = Meta::toStr<T>(v);
                        
                        return true; // flash limits error
                    }
                }
                catch(const std::logic_error &e) { return false; }
            }
            
            return true;
        }
        
        return false;
    }
};

template<typename T>
class MetaTextfield : public Textfield {
public:
    typedef T value_type;
    
public:
    MetaTextfield(const T& value, const Bounds& bounds)
        : Textfield(Meta::toStr<T>(value), bounds)
    { }
    
    T get_value() const {
        try {
            return Meta::fromStr<T>(text());
        } catch(const std::logic_error&) {}
        
        return T();
    }
    
protected:
    virtual bool isTextValid(std::string& text, char inserted, size_t at) override
    {
        try {
            Meta::fromStr<T>(text);
            
            if(!_settings.check_text
               || _settings.check_text(text, inserted, at))
            {
                set_valid(true);
                return true;
            }
            
            set_valid(false);
            return false;
        }
        catch(const illegal_syntax& e) {FormatExcept("illegal_syntax: ", e.what()); set_valid(false); return false;}
        catch(const std::logic_error& e) {FormatExcept("logic_error: ", e.what());}
        
        // syntax error
        set_valid(false);
        return true;
    }
};

void set_clipboard(const std::string& text);
std::string get_clipboard();
}
