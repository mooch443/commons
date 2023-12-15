#pragma once

#include <gui/types/List.h>

namespace gui {
    class TextItem : public List::Item {
    protected:
        GETTER_SETTER(std::string, text);
        
    public:
        TextItem(const std::string& t = "", long idx = -1, bool selected = false)
            : List::Item(idx, selected), _text(t)
        { }
        
        operator const std::string&() const override {
            return _text;
        }
        
        void operator=(const gui::List::Item& other) override {
            gui::List::Item::operator=(other);
            
            assert(dynamic_cast<const TextItem*>(&other));
            _text = static_cast<const TextItem*>(&other)->_text;
        }
    };

    class DetailItem {
        GETTER_SETTER(std::string, name);
        GETTER_SETTER(std::string, detail);
        GETTER_SETTER(bool, disabled);
        
    public:
        DetailItem(const std::string& name = "", const std::string& detail = "", bool disabled = false) : _name(name), _detail(detail), _disabled(disabled) {}
        virtual ~DetailItem(){}
        virtual operator std::string() const {
            return _name;
        }
        virtual bool operator!=(const DetailItem& other) const {
            return _name != other._name || _detail != other._detail;
        }
    };
}
