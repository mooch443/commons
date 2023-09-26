#pragma once
#include <commons.pc.h>
#include <misc/SpriteMap.h>
#include <gui/types/Textfield.h>
#include <gui/GuiTypes.h>
#include <gui/types/Button.h>
#include <gui/types/Dropdown.h>
#include <gui/types/ScrollableList.h>
#include <gui/types/Checkbox.h>
#include <gui/types/List.h>

namespace gui {
namespace dyn {

template<typename T, typename A>
concept takes_attribute = requires(T t, A a) {
    { t.set(a) } -> std::same_as<void>;
};

sprite::Map& settings_map();

struct LabeledField {
    gui::derived_ptr<gui::Text> _text;
    std::string _docs;
    //gui::derived_ptr<gui::HorizontalLayout> _joint;
    
    LabeledField(const std::string& name = "")
    : _text(std::make_shared<gui::Text>(name))
    //_joint(std::make_shared<gui::HorizontalLayout>(std::vector<Layout::Ptr>{_text, _text_field}))
    {
        _text->set_font(Font(0.6f));
        _text->set_color(White);
    }
    
    virtual ~LabeledField() {}
    
    virtual void add_to(std::vector<Layout::Ptr>& v) {
        v.push_back(_text);
    }
    virtual void update() {}
    virtual Layout::Ptr representative() { return _text; }
    virtual void set_description(std::string);
    
    template<typename T>
    void set(T attribute) {
        if constexpr(takes_attribute<Drawable, T>) {
            representative()->set(attribute);
            
        } else if(representative().is<Button>()) {
            if constexpr(takes_attribute<Button, T>)
            {
                representative().to<Button>()->set(attribute);
            }
            
        } else if(representative().is<Textfield>()) {
            if constexpr(takes_attribute<Textfield, T>)
            {
                representative().to<Textfield>()->set(attribute);
            }
            
        } else if(representative().is<Dropdown>()) {
            if constexpr(takes_attribute<Dropdown, T>)
            {
                representative().to<Dropdown>()->set(attribute);
            }
            
        } else if(representative().is<Checkbox>()) {
            if constexpr(takes_attribute<Checkbox, T>)
            {
                representative().to<Checkbox>()->set(attribute);
            }
        } else if(representative().is<List>()) {
            if constexpr(takes_attribute<List, T>)
            {
                representative().to<List>()->set(attribute);
            }
        } else if(representative().is<ScrollableList<>>()) {
            if constexpr(takes_attribute<ScrollableList<>, T>)
            {
                representative().to<ScrollableList<>>()->set(attribute);
            }
        }
    }
    
    static std::unique_ptr<LabeledField> Make(std::string parm, bool invert = false);
    static std::unique_ptr<LabeledField> Make(std::string parm, std::string desc);
};

class Combobox : public Entangled {
public:
    struct Settings {
        Bounds bounds = Bounds(0, 0, 100, 33);
        Color fill_clr = Drawable::accent_color;
        Color line_clr = Black.alpha(200);
        Color text_clr = White;
        Font font = Font(0.75, Align::Center);
        std::string param;
    };
    
protected:
    Settings _settings;
    
    HorizontalLayout _layout;
    derived_ptr<Dropdown> _dropdown;
    std::unique_ptr<LabeledField> _value;
    
public:
    template<typename... Args>
    Combobox(Args... args)
    {
        create(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void create(Args... args) {
        (set(std::forward<Args>(args)), ...);
        init();
    }
    
public:
    using Entangled::set;
    
    void set(attr::Font font);
    void set(attr::FillClr clr) override;
    void set(attr::LineClr clr) override;
    void set(attr::TextClr clr);
    
    void set(std::function<void()> on_click) {
        if(on_click)
            this->on_click([on_click](auto) { on_click(); });
    }
    void set(ParmName name);
    
    void set_bounds(const Bounds& bds) override;
    void set_pos(const Vec2& p) override;
    void set_size(const Size2& p) override;
    
protected:
    void init();
    void update() override;
};

struct LabeledCombobox : public LabeledField {
    gui::derived_ptr<Combobox> _combo;
    LabeledCombobox(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_combo);
    }
    void update() override;
    Layout::Ptr representative() override { return _combo; }
};
struct LabeledTextField : public LabeledField {
    gui::derived_ptr<gui::Textfield> _text_field;
    sprite::Reference _ref;
    LabeledTextField(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_text_field);
    }
    void update() override;
    Layout::Ptr representative() override { return _text_field; }
};
struct LabeledDropDown : public LabeledField {
    gui::derived_ptr<gui::Dropdown> _dropdown;
    sprite::Reference _ref;
    LabeledDropDown(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update() override;
    Layout::Ptr representative() override { return _dropdown; }
};
struct LabeledList : public LabeledField {
    gui::derived_ptr<gui::ScrollableList<>> _list;
    sprite::Reference _ref;
    LabeledList(const std::string& name = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_list);
    }
    void update() override;
    Layout::Ptr representative() override { return _list; }
};
struct LabeledPath : public LabeledField {
    class FileItem {
        GETTER(file::Path, path)
        
    public:
        FileItem(const file::Path& path = "");
        
        Color base_color() const;
        Color color() const;
        operator std::string() const;
        bool operator!=(const FileItem& other) const {
            return _path != other._path;
        }
    };
    
    gui::derived_ptr<gui::Dropdown> _dropdown;
    sprite::Reference _ref;
    
    std::vector<FileItem> _names;
    std::vector<Dropdown::TextItem> _search_items;
    std::set<file::Path, std::function<bool(const file::Path&, const file::Path&)>> _files;
    file::Path _path;
    std::function<bool(file::Path)> _validity;
    
    LabeledPath(std::string name, file::Path path = "");
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update() override;
    void update_names();
    void change_folder(const file::Path&);
    Layout::Ptr representative() override { return _dropdown; }
};
struct LabeledCheckbox : public LabeledField {
    gui::derived_ptr<gui::Checkbox> _checkbox;
    bool _invert{false};
    sprite::Reference _ref;
    LabeledCheckbox(const std::string& name = "", bool invert = false);
    void add_to(std::vector<Layout::Ptr>& v) override {
        //LabeledField::add_to(v);
        v.push_back(_checkbox);
    }
    void update() override;
    Layout::Ptr representative() override { return _checkbox; }
    void set_description(std::string) override;
};

}
}
