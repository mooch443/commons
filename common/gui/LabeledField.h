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
#include <file/PathArray.h>
#include <gui/types/Combobox.h>
#include <gui/GUITaskQueue.h>
#include <gui/Passthrough.h>

namespace cmn::gui {
namespace dyn {

class LayoutContext;
struct State;

template<typename T, typename A>
concept takes_attribute = requires(T t, A a) {
    { t.set(a) } -> std::same_as<void>;
};

sprite::Map& settings_map();

template<typename Derived, typename AttrType, typename = void>
struct has_set : std::false_type {};

template<typename Derived, typename AttrType>
struct has_set<Derived, AttrType, std::void_t<decltype(std::declval<Derived>().derived_set(std::declval<AttrType>()))>> : std::true_type {};

class CustomDropdown : public gui::Dropdown {
    GETTER_SETTER(std::function<void()>, update_callback);
    GETTER_SETTER(derived_ptr<StaticText>, preview);
public:
    using gui::Dropdown::Dropdown;

    void update() override;
    
    using gui::Dropdown::set;
    void set(SizeLimit limit) {
        if(_preview)
            _preview->set(limit);
    }
};

class LabeledField {
private:
    GUITaskQueue_t *_gui{nullptr};
    std::string _docs;
    std::string _ref;
    //sprite::Reference _ref;
    CallbackCollection _callback_id;
    CallbackManagerImpl<void> _delete_callbacks;
    
    mutable std::mutex _ref_mutex;
    
protected:
    auto enqueue(auto fn) {
        if(_gui) {
            return _gui->enqueue(std::forward<decltype(fn)>(fn));
        } else
            throw U_EXCEPTION("No gui set for enqueue.");
    }
    GETTER_NCONST(gui::derived_ptr<gui::Text>, text);
    //gui::derived_ptr<gui::HorizontalLayout> _joint;
    
public:
    sprite::Reference ref() const {
        std::unique_lock guard(_ref_mutex);
        return settings_map()[_ref];
    }
    
    auto register_delete_callback(auto&& fn) {
        return _delete_callbacks.registerCallback(std::forward<decltype(fn)>(fn));
    }
    
    LabeledField(GUITaskQueue_t*, const std::string& name);
    virtual ~LabeledField();
    
    static std::string class_name() { return "Field"; }
    std::string toStr() const;
    virtual void add_to(std::vector<Layout::Ptr>& v) {
        assert(_text != nullptr);
        if(_text->txt().empty())
            return;
        v.push_back(_text);
    }
    
    void trigger_ref_update();
    virtual void update_ref_in_main_thread() {}
    void replace_ref(ParmName name);
    void replace_docs(const std::string&);
    virtual Layout::Ptr representative() const { return _text; }
    virtual void set_description(std::string);
    
    //LabeledField(LabeledField&&) = default;
    LabeledField(const LabeledField&) = delete;
    
    LabeledField& operator=(const LabeledField&) = delete;
    //LabeledField& operator=(LabeledField&&) = default;
    
    virtual std::vector<Layout::Ptr> apply_set() const {
        return { representative() };
    }
    
    template<typename T>
    void set(T attribute) {
        for(auto &ptr : apply_set())
            delegate_to_proper_type(attribute, ptr);
    }
    
    static std::unique_ptr<LabeledField> Make(GUITaskQueue_t*, std::string parm, State&, const LayoutContext&, bool invert = false);
    static std::unique_ptr<LabeledField> Make(GUITaskQueue_t*, std::string parm, bool invert = false);
    
    template<typename T>
    static bool delegate_to_proper_type(const T& attribute, const Layout::Ptr& object)
    {
        if(not object)
            return false;
        
        if(object.is<Fallthrough>()) {
            return delegate_to_proper_type(attribute, object.to<Fallthrough>()->object());
        }
        
        // Existing base class set implementation
        if constexpr(takes_attribute<Drawable, T>) {
            object->set(attribute);
            return true;
        }

        switch (object->type()) {
            case Type::SECTION:
                if constexpr (takes_attribute<Section, T>) {
                    object.to<Section>()->set(attribute);
                }
                break;

            case Type::ENTANGLED:
                if constexpr (takes_attribute<Entangled, T>) {
                    object.to<Entangled>()->set(attribute);
                    return true;
                }
                else if (object.is<Button>()) {
                    if constexpr (takes_attribute<Button, T>) {
                        object.to<Button>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<Textfield>()) {
                    if constexpr (takes_attribute<Textfield, T>) {
                        object.to<Textfield>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<dyn::CustomDropdown>()) {
                    if constexpr (takes_attribute<dyn::CustomDropdown, T>) {
                        object.to<dyn::CustomDropdown>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<Dropdown>()) {
                    if constexpr (takes_attribute<Dropdown, T>) {
                        object.to<Dropdown>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<List>()) {
                    if constexpr (takes_attribute<List, T>) {
                        object.to<List>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<ScrollableList<>>()) {
                    if constexpr (takes_attribute<ScrollableList<>, T>) {
                        object.to<ScrollableList<>>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<ScrollableList<DetailItem>>()) {
                    if constexpr (takes_attribute<ScrollableList<DetailItem>, T>) {
                        object.to<ScrollableList<DetailItem>>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<ScrollableList<DetailTooltipItem>>()) {
                    if constexpr (takes_attribute<ScrollableList<DetailTooltipItem>, T>) {
                        object.to<ScrollableList<DetailTooltipItem>>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<Checkbox>()) {
                    if constexpr (takes_attribute<Checkbox, T>) {
                        object.to<Checkbox>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<Combobox>()) {
                    if constexpr (takes_attribute<Combobox, T>) {
                        object.to<Combobox>()->set(attribute);
                        return true;
                    }
                }
                else if (object.is<Layout>()) {
                    if(object.is<GridLayout>()) {
                        if constexpr (takes_attribute<GridLayout, T>) {
                            object.to<GridLayout>()->set(attribute);
                        }
                    } else {
                        if constexpr (takes_attribute<Layout, T>) {
                            object.to<Layout>()->set(attribute);
                            return true;
                        }
                    }
                }
                else if (object.is<StaticText>()) {
                    if constexpr (takes_attribute<StaticText, T>) {
                        object.to<StaticText>()->set(attribute);
                        return true;
                    }
                }
                break;
            case Type::CIRCLE:
                if constexpr (takes_attribute<Circle, T>) {
                    object.to<Circle>()->set(attribute);
                    return true;
                }
                break;
            case Type::RECT:
                if constexpr (takes_attribute<Rect, T>) {
                    object.to<Rect>()->set(attribute);
                    return true;
                }
                break;
            case Type::VERTICES:
                if constexpr (takes_attribute<VertexArray, T>) {
                    object.to<VertexArray>()->set(attribute);
                    return true;
                }
                break;
            case Type::LINE:
                if constexpr (takes_attribute<Line, T>) {
                    object.to<Line>()->set(attribute);
                    return true;
                }
                break;
            case Type::TEXT:
                if constexpr (takes_attribute<Text, T>) {
                    object.to<Text>()->set(attribute);
                    return true;
                }
                break;

            case Type::IMAGE:
                if constexpr (takes_attribute<ExternalImage, T>) {
                    object.to<ExternalImage>()->set(attribute);
                    return true;
                }
                break;

            default:
                break;
        }

        return false;
    }
};

struct LabeledCombobox : public LabeledField {
    gui::derived_ptr<Combobox> _combo;
    LabeledCombobox(GUITaskQueue_t*, const std::string& name, State&, const glz::json_t& obj);
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_combo);
    }
    Layout::Ptr representative() const override { return _combo; }
    std::optional<std::string> highlighted_parameter() const;
    std::optional<std::string> selected_parameter() const;
};
struct LabeledTextField : public LabeledField {
    gui::derived_ptr<gui::Textfield> _text_field;
    LabeledTextField(GUITaskQueue_t*, const std::string& name, const glz::json_t& obj);
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_text_field);
        assert(_text_field != nullptr);
    }
    void update_ref_in_main_thread() override;
    Layout::Ptr representative() const override { return _text_field; }
};
struct LabeledDropDown : public LabeledField {
    gui::derived_ptr<gui::Dropdown> _dropdown;
    LabeledDropDown(GUITaskQueue_t*, const std::string& name, const glz::json_t& obj);
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update_ref_in_main_thread() override;
    Layout::Ptr representative() const override { return _dropdown; }
};
struct LabeledList : public LabeledField {
    gui::derived_ptr<gui::List> _list;
    bool _invert{false};
    LabeledList(GUITaskQueue_t*, const std::string& name, const glz::json_t& obj, bool invert = false);
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_list);
    }
    void update_ref_in_main_thread() override;
    Layout::Ptr representative() const override { return _list; }
};

struct LabeledPath : public LabeledField {
    class FileItem {
        GETTER(file::Path, path);
        
    public:
        FileItem(const file::Path& path = "");
        
        Color base_color() const;
        Color color() const;
        operator std::string() const;
        bool operator!=(const FileItem& other) const {
            return _path != other._path;
        }
    };
    
    std::mutex _file_mutex;
    std::future<tl::expected<std::vector<Dropdown::TextItem>, const char*>> _file_retrieval;
    std::optional<std::function<std::optional<file::Path>()>> _should_reload;
    
    gui::derived_ptr<CustomDropdown> _dropdown;
    std::vector<FileItem> _names;
    std::vector<Dropdown::TextItem> _search_items;
    std::set<file::Path, std::function<bool(const file::Path&, const file::Path&)>> _files;
    file::Path _path;
    std::optional<file::Path> _folder;
    std::function<bool(file::Path)> _validity;
    
    LabeledPath(GUITaskQueue_t*, std::string name, const std::string& desc, file::Path path);
    void add_to(std::vector<Layout::Ptr>& v) override {
        LabeledField::add_to(v);
        v.push_back(_dropdown);
    }
    void update_ref_in_main_thread() override;
    std::vector<Dropdown::TextItem> updated_names(const std::set<file::Path>& paths);
    void change_folder(file::Path);
    Layout::Ptr representative() const override { return _dropdown; }
    void asyncUpdateItems();
    void asyncRetrieve(std::function<std::optional<file::Path>()>);
};

class LabeledPathArray : public LabeledField {
private:
    gui::derived_ptr<gui::VerticalLayout> _layout;
    gui::derived_ptr<CustomDropdown> _dropdown;
    file::PathArray _pathArray;
    file::PathArray _pathArrayCopy;
    std::vector<file::Path> _tmp_files;
    
    std::vector<Dropdown::TextItem> _search_items;

    std::future<std::vector<file::Path>> _future;
    std::atomic<bool> _should_update{ false };
    
public:
    LabeledPathArray(GUITaskQueue_t*, const std::string& name, const LayoutContext*);
    ~LabeledPathArray();

    void updateStaticText(const std::vector<file::Path>&);
    void updateDropdownItems();
    void add_to(std::vector<Layout::Ptr>& v) override;
    void update_ref_in_main_thread() override;
    
    virtual std::vector<Layout::Ptr> apply_set() const override {
        return {
            Layout::Ptr(_dropdown)//,
            //Layout::Ptr(_staticText)
        };
    }
    Layout::Ptr representative() const override { return _dropdown; }

    void asyncUpdateItems();
};

struct LabeledCheckbox : public LabeledField {
    gui::derived_ptr<gui::Checkbox> _checkbox;
    bool _invert{false};
    LabeledCheckbox(GUITaskQueue_t*, const std::string& name, const std::string& desc, const glz::json_t&, bool invert = false);
    ~LabeledCheckbox();
    void add_to(std::vector<Layout::Ptr>& v) override {
        //LabeledField::add_to(v);
        v.push_back(_checkbox);
    }
    void update_ref_in_main_thread() override;
    Layout::Ptr representative() const override { return _checkbox; }
    void set_description(std::string) override;
};

}
}
