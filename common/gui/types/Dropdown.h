#pragma once

#include <commons.pc.h>
#include <gui/types/Textfield.h>
#include <gui/types/Button.h>
#include <gui/types/ScrollableList.h>

namespace gui {
    struct ClosesAfterSelect {
        bool closes{true};

        ClosesAfterSelect() = default;
        explicit ClosesAfterSelect(bool after) : closes(after) {}

        operator bool() const {
            return closes;
        }
    };

    class Dropdown : public Entangled {
        std::vector<std::string> _corpus;
        PreprocessedData _preprocessed;
        
    public:
        enum Type {
            SEARCH,
            BUTTON
        };
        
        struct FilteredIndex {
            long value;
            explicit FilteredIndex(long v = -1) : value(v) {}

            explicit operator bool() const { return value != -1; }
            
            bool valid() const { return (bool)*this; }
            
            std::string toStr() const { return value != -1 ? Meta::toStr(value) : "null"; }
            static std::string class_name() { return "FilteredIndex"; }

            // Equality operators
            friend bool operator==(const FilteredIndex& lhs, const FilteredIndex& rhs) { return lhs.value == rhs.value; }
            friend bool operator!=(const FilteredIndex& lhs, const FilteredIndex& rhs) { return !(lhs == rhs); }

            // Spaceship operator
            auto operator<=>(const FilteredIndex& other) const { return value <=> other.value; }
        };

        struct RawIndex {
            long value;
            explicit RawIndex(long v = -1) : value(v) {}

            explicit operator bool() const { return value != -1; }

            bool valid() const { return (bool)*this; }
            
            std::string toStr() const { return value != -1 ? Meta::toStr(value) : "null"; }
            static std::string class_name() { return "RawIndex"; }

            // Equality operators
            friend bool operator==(const RawIndex& lhs, const RawIndex& rhs) { return lhs.value == rhs.value; }
            friend bool operator!=(const RawIndex& lhs, const RawIndex& rhs) { return !(lhs == rhs); }

            // Spaceship operator
            auto operator<=>(const RawIndex& other) const { return value <=> other.value; }
        };
        
        class Item {
        protected:
            GETTER(long, ID);
            
        public:
            static constexpr long INVALID_ID = -1;
            
            Item(long ID);
            virtual ~Item() {}
            
            static long automatic_id();
            operator std::string() const {
                return name();
            }
            virtual std::string name() const = 0;
            virtual std::string search_name() const { return name(); }
            virtual std::string display_name() const { return name(); }
        };
        
        class TextItem : public Item {
            std::string _name, _search;
            GETTER_SETTER(std::string, display);
            GETTER_PTR_I(void*, custom, nullptr);
            GETTER_SETTER_I(int, index, Item::INVALID_ID);
            GETTER_SETTER_I(Color, color, Transparent);
            
        public:
            TextItem(const std::string& name = "", long ID = Item::INVALID_ID, const std::string& search = "", void *custom = NULL)
                : Item(ID), _name(name), _search(search), _custom(custom)
            {
                _index = narrow_cast<int>(this->ID());
            }
            
            std::string name() const override {
                return _name;
            }
            
            std::string search_name() const override {
                return _search.empty() ? name() : _search;
            }
            
            std::string display_name() const override {
                return _display.empty() ? name() : _display;
            }
            
            bool operator==(const TextItem& other) const {
                return /*other.ID() == ID() &&*/ other._custom == _custom && other.name() == name();
            }
            bool operator!=(const TextItem& other) const {
                return /*other.ID() != ID() ||*/ other._custom != _custom || other.name() != name();
            }
            
            bool operator<(const TextItem& other) const {
                return ID() < other.ID() || (ID() == other.ID() && other._custom < _custom);
            }
            
            std::string toStr() const;
            static std::string class_name();
            
            static TextItem invalid_item() {
                TextItem item;
                item.set_index(INVALID_ID);
                item._ID = INVALID_ID;
                return item;
            }
        };
        
    protected:
        GETTER_NCONST(std::shared_ptr<Textfield>, textfield){std::make_unique<Textfield>()};
        GETTER_NCONST(std::shared_ptr<Button>, button);
        GETTER_NCONST(ScrollableList<TextItem>, list);
        std::function<void(RawIndex, const TextItem&)> _on_select;
        Textfield::OnEnter_t _on_enter;
        
        GETTER(std::vector<TextItem>, items);
        std::vector<TextItem> _original_items;
        
        GETTER_I(bool, opened, false);
        GETTER_I(Type, type, SEARCH);
        GETTER_I(bool, inverted, false);
        GETTER(ClosesAfterSelect, closes_after_select);
        
        RawIndex _selected_item;
        
        bool _items_changed{false};
        std::map<FilteredIndex, RawIndex> _filtered_items;
        std::map<RawIndex, FilteredIndex> _items_to_filtered_items;
        
        std::function<void(std::string)> _custom_on_text_changed;
        std::function<void(bool)> _on_open;
        
    public:
        using Entangled::set;
        void set(TextClr clr) { _textfield->set(clr); _list.set(clr); }
        void set(LineClr clr) override { Entangled::set(clr); _textfield->set(clr); }
        void set(FillClr clr) override { Entangled::set(clr); _textfield->set(clr); }
        void set(Font font) {
            if(_textfield) _textfield->set(font);
            if(_button) _button->set(font);
        }
        void set(ListDims_t dims) { _list.set(dims); }
        void set(LabelDims_t dims) { _list.set(dims); }
        void set(ListFillClr_t dims) { _list.set(dims); }
        void set(ListLineClr_t dims) { _list.set(dims); }
        void set(LabelColor_t dims) { _list.set(dims); }
        void set(LabelBorderColor_t dims) { _list.set(dims); }
        void set(ClosesAfterSelect closes) { _closes_after_select = closes; set_content_changed(true); }
        void set(const std::vector<TextItem>& options) { set_items(options); }
        void set(const std::vector<std::string>& options) { set_items(std::vector<TextItem>(options.begin(), options.end())); }
        void set(const Type& type) { _type = type; set_content_changed(true); }
        void set(std::function<void(RawIndex, const TextItem&)> on_select) {
            _on_select = on_select;
        }
        void set(Textfield::OnEnter_t on_enter) {
            _on_enter = on_enter;
        }
        
        void set_bounds(const Bounds& bounds) override;
        void set_size(const Size2& size) override;
        
        const auto& on_open() const { return _on_open; }
        const auto& on_enter() const { return _on_enter; }
        const auto& on_select() const { return _on_select; }
        
        template<typename... Args>
        Dropdown(Args... args) {
            create(std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        void create(Args... args) {
            init();
            (set(std::forward<Args>(args)), ...);
        }
        
        void init();
        
        ~Dropdown();
        
        void set_opened(bool opened);
        
        void on_select(const std::function<void(RawIndex, const TextItem&)>& fn) {
            _on_select = fn;
        }
        
        void on_open(const std::function<void(bool)>& fn) {
            _on_open = fn;
        }
        
        void on_text_changed(const std::function<void(std::string)>& fn) {
            _custom_on_text_changed = fn;
        }
        
        void select_textfield();
        void clear_textfield();
        
        void set_items(const std::vector<TextItem>& options);
        const std::string& text() const;
        TextItem selected_item() const;
        TextItem hovered_item() const;
        void select_item(RawIndex index);
        bool has_selection() const;
        RawIndex filtered_item_index(FilteredIndex index) const;
        
    protected:
        void set_inverted(bool); //! whether dropdown opens to the top
        void _set_open(bool);
        void update() override;
        void update_bounds() override;
        void on_text_changed();
        void apply_item_filtering();
    };
}
