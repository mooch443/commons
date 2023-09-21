#include "Dropdown.h"

namespace gui {

auto convert_to_search_name(const std::vector<Dropdown::TextItem>& items) {
    std::vector<Dropdown::TextItem> result;
    result.reserve(items.size());
    for(auto&item : items)
        result.emplace_back(item.display_name());
    return result;
}

std::string Dropdown::TextItem::toStr() const {
    if(search_name() != name() || display_name() != name())
        return "Item<name='"+name()+"' search='"+search_name()+"' display='"+display_name()+"'>";
    return "Item<'"+name()+"'>";
}
std::string Dropdown::TextItem::class_name() {
    return "Dropdown::TextItem";
}

    std::atomic_long _automatic_id = 0;
    Dropdown::Item::Item(long ID) : _ID(ID) {
        if(_ID == Item::INVALID_ID) {
            _ID = automatic_id();
        } else
            ++_automatic_id;
    }
    
    long Dropdown::Item::automatic_id() {
        return _automatic_id++;
    }
    
void Dropdown::init() {
    _list = std::make_unique<ScrollableList<TextItem>>(Bounds(0, _bounds.height, _bounds.width, 230), items(), Font(0.6f, Align::Left), [this](size_t s) { _selected_item = s; });
    _list->set_z_index(1);
    _selected_item = -1;
    
    if(_type == BUTTON) {
        _button = Button::MakePtr("Please select...",  Bounds(_bounds.size()));
        _button->set_toggleable(true);
        _button->add_event_handler(MBUTTON, [this](Event e){
            if(!e.mbutton.pressed && e.mbutton.button == 0) {
                _opened = !_opened;
                if(_opened) {
                    _textfield->set_z_index(2);
                } else {
                    _textfield->set_z_index(0);
                }
                this->set_content_changed(true);
                if(_on_open)
                    _on_open(_opened);
            }
        });
        
    } else {
        _textfield = std::make_shared<Textfield>(Bounds(_bounds.size()));
        _textfield->set_placeholder("Please select...");
        _textfield->add_event_handler(KEY, [this](Event e){
            if(!e.key.pressed)
                return;
            
            if(e.key.code == Codes::Down) {
                auto& items = _list->items();
                if(not items.empty() && _selected_item + 1 < _list->items().size() && _selected_item != -1) {
                    _selected_item++;
                } else if(not items.empty()) {
                    _selected_item = 0;
                } else
                    _selected_item = -1;
                
                _list->highlight_item(_selected_item);
            }
            else if(e.key.code == Codes::Up) {
                if(_selected_item > 0 && _selected_item < _list->items().size()) {
                    _selected_item--;
                } else if(not _list->items().empty()) {
                    _selected_item = _list->items().size() - 1;
                } else
                    _selected_item = -1;
                
                _list->highlight_item(_selected_item);
            }
            
            select_item(_selected_item);
        });
        
        _on_text_changed = [this](){
            if (_custom_on_text_changed) {
                auto text = _textfield->text();
                _custom_on_text_changed(text);
            }

            filtered_items.clear();

            if (!_textfield->text().empty()) {
                if(_corpus.empty()) {
                    _corpus.reserve(items().size());
                    for (const auto &item : _items) {
                        _corpus.push_back(item.search_name());
                    }
                }
                
                if(_preprocessed.empty())
                    _preprocessed = preprocess_corpus(_corpus);
                
                // Filter items using the search text
                // Get the search result indexes
                auto search_result_indexes = text_search(_textfield->text(), _corpus, _preprocessed);

                // Create a filtered vector of TextItems
                std::vector<TextItem> filtered;
                for (int index : search_result_indexes) {
                    filtered.push_back(_items[index]);
                    filtered_items[filtered.size() - 1] = index;
                }

                // Update the list with the filtered items
                if(_list->set_items(convert_to_search_name(filtered))) {
                //if(not filtered.empty()) {
                    select_item(-1);
                    _list->set_scroll_offset(Vec2());
                }

            } else {
                // If the search text is empty, display all items
                _list->set_items(convert_to_search_name(_items));
            }

            this->select_item(_selected_item);
            this->set_content_changed(true);
        };
        
        _textfield->on_text_changed(_on_text_changed);
        
        _textfield->on_enter([this](){
            if(!_list->items().empty()) {
                if (stage())
                    stage()->select(NULL);
                _list->select_highlighted_item();
            } else {
                if(stage())
                    stage()->select(NULL);
                _selected_id = -1;
                if(_on_select)
                    _on_select(-1, TextItem());
            }
            
            if(stage())
                stage()->do_hover(NULL);
            
            _list->set_last_hovered_item(-1);
        });
    }
    
    _list->on_select([this](size_t i, Dropdown::TextItem txt){
        long_t real_id = long_t(i);
        if(!filtered_items.empty()) {
            if(filtered_items.find(i) != filtered_items.end()) {
                real_id = long_t(filtered_items[i]);
            } else
                throw U_EXCEPTION("Unknown item id ",i," (",filtered_items.size()," items)");
        }
        
        txt = _items.at(real_id);
        
        if(_button) {
            _button->set_toggle(_opened);
            _button->set_txt(txt);
            
        } else if(_textfield) {
            _textfield->set_text(txt);
        }
        
        _selected_id = real_id;
        
        /*if(stage()) {
            stage()->select(NULL);
            stage()->do_hover(NULL);
        }*/
        
        _list->set_last_hovered_item(-1);
        
        if(this->selected() != _opened) {
            _opened = this->selected();
            if(_opened) {
                _textfield->set_z_index(2);
            } else {
                _textfield->set_z_index(0);
            }
            if(_on_open)
                _on_open(_opened);
        }
        
        this->set_content_changed(true);
        
        if(_on_select)
            _on_select(real_id, txt);
    });
    
    set_clickable(true);
    
    if(_type == SEARCH)
        add_event_handler(SELECT, [this](Event e) {
            if(e.select.selected)
                this->set_opened(true);
            else
                this->set_opened(false);
            if(this->_on_open)
                this->_on_open(this->_opened);
        });
    }

    Dropdown::~Dropdown() {
        _button = nullptr;
        _textfield = nullptr;
    }
    
    void Dropdown::set_opened(bool opened) {
        if(_opened == opened)
            return;
        
        _opened = opened;
        _selected_item = -1;
        if(_opened) {
            _textfield->set_z_index(2);
        } else {
            _textfield->set_z_index(0);
        }
        set_content_changed(true);
    }
    
    void Dropdown::select_textfield() {
        if(stage() && _textfield) {
            stage()->select(_textfield.get());
        }
    }
    void Dropdown::clear_textfield() {
        if(_textfield) {
            _textfield->set_text("");
            if(_on_text_changed)
                _on_text_changed();
        }
    }
    
    void Dropdown::update() {
        if(!content_changed())
            return;
        
        begin();
        if(_opened)
            advance_wrap(*_list);
        if(_button)
            advance_wrap(*_button);
        else
            advance_wrap(*_textfield);
        end();
    }
    
    void Dropdown::select_item(long index) {
        _selected_item = index;
        
        if(index >= 0 && _list && (size_t)index+1 > _list->items().size())
            index = (long)_list->items().size()-1;
        
        if(not _list || _list->items().empty())
            index = -1;
        
        //if(index > -1 && _list)
        //    _list->highlight_item(index);
        
        if(index != _selected_item) {
            _selected_item = index;
            set_dirty();
        }
    }
    
    void Dropdown::set_items(const std::vector<TextItem> &options) {
        if(options != _items) {
            if(_list) {
                _list->set_items(convert_to_search_name(options));
                print("Setting items = ", items());
            }
            _items = options;
            _selected_id = _selected_item = -1;
            _preprocessed = {};
            _corpus.clear();
            if(_on_text_changed)
                _on_text_changed();
            set_dirty();
        }
    }
    
    const std::string& Dropdown::text() const {
        if(!_textfield)
            throw U_EXCEPTION("No textfield.");
        return _textfield->text();
    }
    
    Dropdown::TextItem Dropdown::selected_item() const {
        if(selected_id() != -1)
            return _list && (size_t)_selected_item < _list->items().size() ?_list->items().at(_selected_item).value() : TextItem();
        
        return TextItem();
    }
    
    Dropdown::TextItem Dropdown::hovered_item() const {
        if(_list && _list->last_hovered_item() != -1) {
            return (size_t)_list->last_hovered_item() < _list->items().size() ? _list->items().at(_list->last_hovered_item()).value() : TextItem();
        }
        return TextItem();
    }
    
    void Dropdown::update_bounds() {
        if(!bounds_changed())
            return;
        
        Entangled::update_bounds();
        
        if(stage()) {
            auto &gb = global_bounds();
            if(gb.y >= stage()->height()/stage()->scale().y * 0.5) {
                set_inverted(true);
            } else
                set_inverted(false);
        }
        
        if(_textfield)
            _textfield->set_size(Size2(width(), height()).div(scale()));
        if(_list) {
            _list->set_bounds(Bounds(0, _inverted ? -_list->height() : height(), width() / scale().x, _list->height()));
        }
    }
    
    void Dropdown::set_inverted(bool invert) {
        if(_inverted == invert)
            return;
        
        _inverted = invert;
        if(_inverted)
            _list->set_pos(-Vec2(0, _list->height()));
        else
            _list->set_pos(Vec2(0, height()));
        
        set_content_changed(true);
    }

void Dropdown::set_bounds(const Bounds &bounds) {
    if(not this->bounds().Equals(bounds)) {
        Entangled::set_bounds(bounds);
        set_content_changed(true);
    }
}

void Dropdown::set_size(const Size2 &size) {
    if(not this->bounds().size().Equals(size)) {
        Entangled::set_size(size);
        set_content_changed(true);
    }
}

}
