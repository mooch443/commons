#pragma once
#include <commons.pc.h>

namespace gui {
    class List;
    class Rect;

    class Item {
    protected:
        GETTER_SETTER(long, ID);
        bool _selected;
        gui::List *_list;
        
    protected:
        friend class List;
        
    public:
        Item(long ID = -1, bool selected = false) : _ID(ID), _selected(selected), _list(NULL) {}
        virtual ~Item() {}
        
        bool operator==(const long& other) const {
            return _ID == other;
        }
        
        virtual bool operator==(const Item& other) const {
            return other.ID() == ID();
        }
        virtual operator const std::string&() const = 0;
        virtual void operator=(const Item& other);
        virtual void set_selected(bool selected) { _selected = selected; }
        virtual bool selected() const { return _selected; }
        virtual void update() {}
        
        void convert(std::shared_ptr<Rect> r) const;
    };

    class TextItem : public Item {
    protected:
        GETTER_SETTER(std::string, text);
        
    public:
        TextItem(const std::string& t = "", long idx = -1, bool selected = false)
            : Item(idx, selected), _text(t)
        { }
        
        operator const std::string&() const override {
            return _text;
        }
        
        void operator=(const Item& other) override {
            Item::operator=(other);
            
            assert(dynamic_cast<const TextItem*>(&other));
            _text = static_cast<const TextItem*>(&other)->_text;
        }
    };

    class DetailItem {
        GETTER_SETTER(std::string, name);
        GETTER_SETTER(std::string, detail);
        GETTER_SETTER(bool, disabled);
        
    public:
        DetailItem(const std::string& name = "", const std::string& detail = "", bool disabled = false);
        virtual ~DetailItem(){}
        virtual operator std::string() const {
            return _name;
        }
        virtual bool operator!=(const DetailItem& other) const;
    };
}
