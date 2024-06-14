#include "ListItemTypes.h"
#include <misc/GlobalSettings.h>
#include <gui/GuiTypes.h>
#include <gui/types/List.h>

namespace cmn::gui {
void Item::operator=(const Item& other) {
    _ID = other._ID;
    if(other.selected())
        _selected = other.selected();
}

void Item::convert(std::shared_ptr<Rect> r) const {
    assert(_list);
    
    r->clear_event_handlers();
    r->set_clickable(true);
    r->add_event_handler(MBUTTON, [this](Event e) {
        if(e.mbutton.pressed || e.mbutton.button != 0)
            return;
        
        _list->on_click(this);
        _list->toggle_item(_ID);
    });
    
    if(_selected) {
        _list->set_selected_rect(std::move(r));
    }
}

DetailItem::DetailItem(const std::string& name,
                       const std::string& detail,
                       bool disabled)
    : _name(name), _detail(detail), _disabled(disabled)
{}

bool DetailItem::operator!=(const DetailItem& other) const {
    return _name != other._name || _detail != other._detail;
}

}
