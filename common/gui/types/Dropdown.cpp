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
    _list = std::make_unique<ScrollableList<TextItem>>(Bounds(0, _bounds.height, _bounds.width, 230), items(), Font(0.6f, Align::Left), [this](size_t s) {
        FilteredIndex filtered_index(s); // explicit casting to FilteredIndex
        if(filtered_items.contains(filtered_index)) {
            _selected_item = RawIndex(filtered_items.at(filtered_index).value); // Use RawIndex for type safety
        } else {
            FormatWarning("Invalid ItemID selected: ", s, " for ", extract_keys(filtered_items));
        }
    });
    _selected_item = RawIndex();
    _list->set_z_index(1);
    
    if(_type == BUTTON) {
        _button = Button::MakePtr("Please select...",  attr::Box(_bounds.size()));
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
        _textfield = std::make_shared<Textfield>(Box(_bounds.size()));
        _textfield->set_placeholder("Please select...");
        _textfield->add_event_handler(KEY, [this](Event e){
            if(!e.key.pressed)
                return;
            
            if(e.key.code == Codes::Down) {
                auto& items = _list->items();
                FilteredIndex current = items_to_filtered_items.contains(_selected_item) ? items_to_filtered_items.at(_selected_item) : FilteredIndex{};

                if (current && size_t(current.value + 1) < items.size()) {
                    // Get the next filtered item
                    if (filtered_items.contains(FilteredIndex{current.value + 1})) {
                        _selected_item = filtered_items.at(FilteredIndex{current.value + 1});
                    }
                } else if (filtered_items.contains(FilteredIndex{0})) {
                    // Start from the beginning
                    _selected_item = filtered_items.at(FilteredIndex{0});
                } else {
                    _selected_item = RawIndex{};
                }

                if (items_to_filtered_items.contains(_selected_item)) {
                    _list->highlight_item(items_to_filtered_items.at(_selected_item).value);
                } else {
                    _list->highlight_item(-1);
                }
            }
            else if(e.key.code == Codes::Up) {
                FilteredIndex current = items_to_filtered_items.contains(_selected_item) ? items_to_filtered_items.at(_selected_item) : FilteredIndex();

                if (current) {
                    // Find the previous filtered item
                    auto iter = filtered_items.find(FilteredIndex{current.value - 1});
                    if (iter != filtered_items.end()) {
                        _selected_item = iter->second;
                    }
                } else if (!filtered_items.empty()) {
                    // Go to the last item if we're at the beginning
                    _selected_item = filtered_items.rbegin()->second;
                } else {
                    _selected_item = RawIndex{};
                }

                if (items_to_filtered_items.contains(_selected_item)) {
                    _list->highlight_item(items_to_filtered_items.at(_selected_item).value);
                } else {
                    _list->highlight_item(-1);
                }
            }
            
            select_item(_selected_item);
        });
        
        _on_text_changed = [this](){
            if (_custom_on_text_changed) {
                auto text = _textfield->text();
                _custom_on_text_changed(text);
            }

            filtered_items.clear();
            items_to_filtered_items.clear();

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
                for (int _index : search_result_indexes) {
                    RawIndex index(_index);
                    filtered.push_back(_items[_index]);
                    filtered_items[FilteredIndex{static_cast<long>(filtered.size() - 1)}] = index;
                    items_to_filtered_items[index] = FilteredIndex{static_cast<long>(filtered.size() -1)};
                }

                // Update the list with the filtered items
                if(_list->set_items(convert_to_search_name(filtered))) {
                //if(not filtered.empty()) {
                    select_item(RawIndex{});
                    _list->set_scroll_offset(Vec2());
                }

            } else {
                // If the search text is empty, display all items
                // trivial mapping
                for(long i=0; i<(long)_items.size(); ++i) {
                    filtered_items[FilteredIndex{i}] = RawIndex{i};
                    items_to_filtered_items[RawIndex{i}] = FilteredIndex{i};
                }
                _list->set_items(convert_to_search_name(_items));
            }

            this->select_item(_selected_item);
            this->set_content_changed(true);
        };
        
        _textfield->on_text_changed(Textfield::OnTextChanged_t(_on_text_changed));
        
        _textfield->on_enter([this](){
            if(_on_enter) {
                _on_enter();
                return;
            }
            
            if(!_list->items().empty()) {
                if(_closes_after_select) {
                    if (stage())
                        stage()->select(NULL);
                }
                _list->select_highlighted_item();
            } else {
                if(_closes_after_select) {
                    if(stage())
                        stage()->select(NULL);
                }
                //_selected_item = -1;
                if(_on_select)
                    _on_select(RawIndex{}, TextItem::invalid_item());
            }
            
            if(stage())
                stage()->do_hover(NULL);
            
            _list->set_last_hovered_item(-1);
        });
    }
    
    _list->on_select([this](size_t i, Dropdown::TextItem txt){
        FilteredIndex filtered_id{long_t(i)};
        RawIndex real_id;
        if(!filtered_items.empty()) {
            if(filtered_items.find(filtered_id) != filtered_items.end()) {
                real_id = filtered_items.at(filtered_id);
            } else
                throw U_EXCEPTION("Unknown item id ",i," (",filtered_items.size()," items)");
        }
        
        if(real_id.valid())
            txt = _items.at(real_id.value);
        
        if(_button) {
            _button->set_toggle(_opened);
            _button->set_txt(txt);
            
        } else if(_textfield) {
            _textfield->set_text(txt);
        }
        
        //_selected_id = real_id;
        
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
        _selected_item = RawIndex{};
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

Dropdown::RawIndex Dropdown::filtered_item_index(FilteredIndex index) const {
    if(filtered_items.contains(index)) {
        return filtered_items.at(index);
    }
    return RawIndex{};
}
    
    void Dropdown::select_item(RawIndex index) {
        if(static_cast<size_t>(index.value) < items().size())
            print("Selecting item ", items().at(index.value), " at index ", index.value, " with filtered ", filtered_items, ". items_to_filtered = ", items_to_filtered_items);

        if (index.value >= 0 && items_to_filtered_items.contains(index)) {
            auto iter = items_to_filtered_items.find(index);
            if (iter != items_to_filtered_items.end()) {
                _selected_item = index; // Use RawIndex for type safety
                if(_list)
                    _list->highlight_item(iter->second.value); // Highlight based on filtered list
            } else {
                _selected_item = RawIndex(); // Using default constructor
            }
        } else {
            _selected_item = RawIndex(); // Using default constructor
        }

        if (_selected_item.value != index.value) {
            print("Setting _selected_item = ", _selected_item.value, " from ", index.value, " in ", (uint64_t)this);
            set_dirty();
        }
    }

    bool Dropdown::has_selection() const {
        return (bool)_selected_item;
    }
    
    void Dropdown::set_items(const std::vector<TextItem> &options) {
        if(options != _items) {
            print("_items=",_items, " options=",options);
            _items.clear();
            
            size_t i=0;
            for(auto& o : options) {
                //if(o.ID() == Item::INVALID_ID)
                //{
                    _items.emplace_back(o.name(), i, o.search_name(), o.custom());
                //} else {
                //    _items.emplace_back(o);
                //}
                ++i;
            }
            //_items = options;
            if(_list) {
                _list->set_items(convert_to_search_name(_items));
                print("Setting items = ", items());
            }
            _selected_item = RawIndex{};
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
        if(_selected_item)
            return size_t(_selected_item.value) < _items.size() ? _items.at(_selected_item.value) : TextItem::invalid_item();
            //return _list && (size_t)_selected_item < _list->items().size() ?_list->items().at(_selected_item).value() : TextItem();
        
        return TextItem::invalid_item();
    }
    
    Dropdown::TextItem Dropdown::hovered_item() const {
        if(_list && _list->last_hovered_item() != -1) {
            auto index = FilteredIndex{(long_t)_list->last_hovered_item()};
            if(filtered_items.contains(index)) {
                RawIndex raw = filtered_items.at(index);
                if(raw.value < _items.size()) {
                    return _items.at(raw.value);
                }
            }
            //return (size_t)_list->last_hovered_item() < _list->items().size() ? _list->items().at(_list->last_hovered_item()).value() : TextItem();
        }
        return TextItem::invalid_item();
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
