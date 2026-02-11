#include "Dropdown.h"

namespace cmn::gui {

auto convert_to_search_name(const std::vector<Dropdown::TextItem>& items) {
    std::vector<Dropdown::TextItem> result;
    result.reserve(items.size());
    for (auto& item : items) {
        Dropdown::TextItem p(item.display_name() ? item.display_name().value() : item.name());
        p.set_color(item.color());
        result.emplace_back(std::move(p));
    }
    return result;
}

std::string Dropdown::TextItem::toStr() const {
    if(ID() == INVALID_ID)
        return "Item<null>";
    if(search_name() || display_name())
        return "Item<name="+Meta::toStr(name())+" search="+Meta::toStr(search_name())+" display="+Meta::toStr(display_name())+">";
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
    _list.create(Box(0, _bounds.height, _bounds.width, 230), items(), CornerFlags_t{true, true, true, false}, ItemFont_t(0.6f, Align::Left), [this](size_t s) {
        FilteredIndex filtered_index(s); // explicit casting to FilteredIndex
        if(_filtered_items.contains(filtered_index)) {
            _selected_item = RawIndex(_filtered_items.at(filtered_index).value); // Use RawIndex for type safety
        }
#ifndef NDEBUG
        else {
            FormatWarning("Invalid ItemID selected: ", s, " for ", extract_keys(_filtered_items));
        }
#endif
    });
    _selected_item = RawIndex();
    _items_changed = true;
    set_content_changed(true);
    set_opened(false);
    
    if(_type == BUTTON) {
        _button = Button::MakePtr(Str{"Please select..."},  attr::Box(_bounds.size()));
        _button->set_toggleable(true);
        _button->add_event_handler(MBUTTON, [this](Event e){
            if(!e.mbutton.pressed && e.mbutton.button == 0) {
                _set_open(not _opened);
            }
        });
        
    } else {
        _textfield->set(Box(_bounds.size()));
        _textfield->set_placeholder("Please select...");
        _textfield->add_event_handler(KEY, [this](Event e){
            if(!e.key.pressed)
                return;
            
            if(not is_in(e.key.code, Codes::Up, Codes::Down))
                return;
            
            if(e.key.code == Codes::Down) {
                _set_open(true);
                
                auto& items = _list.items();
                FilteredIndex current = _items_to_filtered_items.contains(_selected_item) ? _items_to_filtered_items.at(_selected_item) : FilteredIndex{};
                
                if (current && size_t(current.value + 1) < items.size()) {
                    // Get the next filtered item
                    if (_filtered_items.contains(FilteredIndex{current.value + 1})) {
                        _selected_item = _filtered_items.at(FilteredIndex{current.value + 1});
                    }
                } else if (_filtered_items.contains(FilteredIndex{0})) {
                    // Start from the beginning
                    _selected_item = _filtered_items.at(FilteredIndex{0});
                } else {
                    _selected_item = RawIndex{};
                }
                
                if (_items_to_filtered_items.contains(_selected_item)) {
                    _list.highlight_item(_items_to_filtered_items.at(_selected_item).value);
                } else {
                    _list.highlight_item(-1);
                }
            }
            else if(e.key.code == Codes::Up) {
                _set_open(true);
                
                FilteredIndex current = _items_to_filtered_items.contains(_selected_item) ? _items_to_filtered_items.at(_selected_item) : FilteredIndex();
                
                if (current) {
                    // Find the previous filtered item
                    auto iter = _filtered_items.find(FilteredIndex{current.value - 1});
                    if (iter != _filtered_items.end()) {
                        _selected_item = iter->second;
                    }
                } else if (!_filtered_items.empty()) {
                    // Go to the last item if we're at the beginning
                    _selected_item = _filtered_items.rbegin()->second;
                } else {
                    _selected_item = RawIndex{};
                }
                
                if (_items_to_filtered_items.contains(_selected_item)) {
                    _list.highlight_item(_items_to_filtered_items.at(_selected_item).value);
                } else {
                    _list.highlight_item(-1);
                }
            }
            
            select_item(_selected_item);
        });
        
        _textfield->on_text_changed(Textfield::OnTextChanged_t([this](){
            on_text_changed();
        }));
        
        _textfield->set(Textfield::OnClearText_t([this]{
            _set_open(true);
            if(stage())
                stage()->select(_textfield.get());
            if(_on_clear)
                _on_clear();
        }));
        
        _textfield->on_enter([this](){
            if(_on_enter) {
                _on_enter();
                return;
            }
            
            if(not _list.items().empty()) {
                if(_closes_after_select) {
                    if (stage())
                        stage()->select(NULL);
                }
                _list.select_highlighted_item();
            } else {
                if(_closes_after_select) {
                    if(stage())
                        stage()->select(NULL);
                }
                //_selected_item = -1;
                if(_on_select)
                    _on_select(RawIndex{}, TextItem::invalid_item());
            }
            
            //if(stage())
            //    stage()->do_hover(NULL);
            if(_closes_after_select)
                _set_open(false);
            _list.set_last_hovered_item(-1);
        });
    }
    
    _list.on_select([this](size_t i, Dropdown::TextItem txt){
        FilteredIndex filtered_id{long_t(i)};
        RawIndex real_id;
        if(not _filtered_items.empty()) {
            if(auto it = _filtered_items.find(filtered_id);
               it != _filtered_items.end())
            {
                real_id = it->second;
            } else
                throw U_EXCEPTION("Unknown item id ",i," (",_filtered_items.size()," items)");
        }
        
        if(real_id.valid())
            txt = _items.at(real_id.value);
        
        if(_button) {
            _button->set_toggle(_opened);
            _button->set_txt(txt);
            
        } else if(_textfield) {
            _textfield->set_text(txt);
        }
        
        _set_open(false);
        _list.set_last_hovered_item(-1);
        
        this->set_content_changed(true);
        
        if(_on_select)
            _on_select(real_id, txt);
    });
    
    set_clickable(true);
    
    if(_type == SEARCH) {
        
        add_event_handler(SELECT, [this](Event e) {
            if(not e.select.selected) {
                _set_open(e.select.selected);
            } else if(not _opened && selected()) {
                _set_open(true);
            }
        });
    }
}

void Dropdown::set(Textfield::OnClearText_t c) {
    _on_clear = c;
}

void Dropdown::set(CornerFlags_t flags) {
    _list_corner_flags = flags;
    if(_opened) {
        if(_inverted) {
            flags.set(CornerFlags::Corner::BottomLeft, false);
            flags.set(CornerFlags::Corner::BottomRight, false);
        } else {
            flags.set(CornerFlags::Corner::TopLeft, false);
            flags.set(CornerFlags::Corner::TopRight, false);
        }
    }
    _list.set(flags);
}

void Dropdown::set(LabelCornerFlags flags) {
    _label_corner_flags = (CornerFlags_t)(CornerFlags)flags;
    
    CornerFlags_t push{_label_corner_flags};
    if(_opened) {
        if(_inverted) {
            push.set(CornerFlags::Corner::TopLeft, false);
            push.set(CornerFlags::Corner::TopRight, false);
        } else {
            push.set(CornerFlags::Corner::BottomLeft, false);
            push.set(CornerFlags::Corner::BottomRight, false);
        }
    }
    _list.set((LabelCornerFlags)(CornerFlags)push);
    
    _list.set(flags);
    if(_button)
        _button->set(push);
    if(_textfield)
        _textfield->set(push);
    Entangled::set(push);
}

void Dropdown::set(LabelFillClr_t dims) {
    _list.set(dims);
}
void Dropdown::set(LabelColor_t color) {
    if(_textfield)
        _textfield->set(TextClr{(Color)color});
}
void Dropdown::set(LabelBorderColor_t dims) {
    _list.set(dims);
    if(_textfield)
        _textfield->set(LineClr{(Color)dims});
}

void Dropdown::set(Placeholder_t placeholder) {
    _list.set(placeholder);
}

    Dropdown::~Dropdown() {
        _button = nullptr;
        _textfield = nullptr;
    }

    void Dropdown::_set_open(bool open) {
        if(open != _opened) {
            set_opened(open);
            if(this->_on_open)
                this->_on_open(open);
        }
    }
    
    void Dropdown::set_opened(bool opened) {
        if(_opened == opened)
            return;
        
        _opened = opened;
        //_selected_item = RawIndex{};
        if(_opened) {
            set_z_index(2);
        } else {
            set_z_index(0);
            if(selected())
                if(stage()) stage()->select(nullptr);
            if(hovered())
                if(stage()) stage()->do_hover(nullptr);
        }
        
        set((LabelCornerFlags)(CornerFlags)_label_corner_flags);
        set(_list_corner_flags);
        set_content_changed(true);
    }
    
    void Dropdown::select_textfield() {
        if(stage() && _textfield) {
            stage()->select(_textfield.get());
        }
    }
    void Dropdown::clear_textfield() {
        if(_textfield && not _textfield->text().empty()) {
            _textfield->set_text("");
            on_text_changed();
        }
    }
    
    void Dropdown::update() {
        apply_item_filtering();
        
        if(!content_changed())
            return;
        
        auto ctx = OpenContext();
        if(_opened)
            advance_wrap(_list);
        if(_button)
            advance_wrap(*_button);
        else
            advance_wrap(*_textfield);
    }

Dropdown::RawIndex Dropdown::filtered_item_index(FilteredIndex index) const {
    if(auto it = _filtered_items.find(index);
       it != _filtered_items.end())
    {
        return it->second;
    }
    return RawIndex{};
}
    
    void Dropdown::select_item(RawIndex index) {
        //if(static_cast<size_t>(index.value) < items().size())
        //    Print("Selecting item ", items().at(index.value), " at index ", index.value, " with filtered ", filtered_items, ". items_to_filtered = ", items_to_filtered_items);

        if (index.value >= 0 && _items_to_filtered_items.contains(index)) {
            if (auto iter = _items_to_filtered_items.find(index);
                iter != _items_to_filtered_items.end())
            {
                _selected_item = index; // Use RawIndex for type safety
                _list.highlight_item(iter->second.value); // Highlight based on filtered list
            } else {
                _selected_item = RawIndex(); // Using default constructor
            }
            
        } else {
            _selected_item = RawIndex(); // Using default constructor
        }

        if (_selected_item.value != index.value) {
            Print("Setting _selected_item = ", _selected_item.value, " from ", index.value, " in ", (uint64_t)this);
            set_dirty();
        }
    }

    bool Dropdown::has_selection() const {
        return (bool)_selected_item;
    }
    
    void Dropdown::set_items(const std::vector<TextItem> &options) {
        if(options == _original_items)
            return;
        
        _original_items = options;
        _items.clear();
        
        size_t i=0;
        for(auto& o : options) {
            //if(o.ID() == Item::INVALID_ID)
            //{
                _items.emplace_back(o.name(), i, o.search_name(), o.custom(), o.docs());
                _items.back().set_color(o.color());
            //} else {
            //    _items.emplace_back(o);
            //}
            ++i;
        }
        //_items = options;
        _list.set_items(convert_to_search_name(_items));
        _selected_item = RawIndex{};
        _preprocessed.reset();
        _corpus.clear();
        //if(_on_text_changed)
        //    _on_text_changed();
        _items_changed = true;
        set_dirty();
    }
    
    const std::string& Dropdown::text() const {
        if(!_textfield)
            throw U_EXCEPTION("No textfield.");
        return _textfield->text();
    }
    
    Dropdown::TextItem Dropdown::selected_item() const {
        if(_selected_item)
            return size_t(_selected_item.value) < _items.size() ? _items.at(_selected_item.value) : TextItem::invalid_item();
            //return _list && (size_t)_selected_item < _list.items().size() ?_list.items().at(_selected_item).value() : TextItem();
        
        return TextItem::invalid_item();
    }
    
    Dropdown::TextItem Dropdown::hovered_item() const {
        if(_list.last_hovered_item() != -1) {
            auto index = FilteredIndex{(long_t)_list.last_hovered_item()};
            if(_filtered_items.contains(index)) {
                RawIndex raw = _filtered_items.at(index);
                if(raw.valid() && static_cast<size_t>(raw.value) < _items.size())
                    return _items.at(raw.value);
            }
            //return (size_t)_list.last_hovered_item() < _list.items().size() ? _list.items().at(_list.last_hovered_item()).value() : TextItem();
        }
        return TextItem::invalid_item();
    }

std::optional<Dropdown::TextItem> Dropdown::currently_hovered_item() const {
    if(_list.currently_highlighted_item() != -1) {
        auto index = FilteredIndex{(long_t)_list.currently_highlighted_item()};
        if(_filtered_items.contains(index)) {
            RawIndex raw = _filtered_items.at(index);
            if(raw.valid() && static_cast<size_t>(raw.value) < _items.size())
                return _items.at(raw.value);
        }
        //return (size_t)_list.last_hovered_item() < _list.items().size() ? _list.items().at(_list.last_hovered_item()).value() : TextItem();
    }
    return std::nullopt;
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
         
            //_list.set(LabelDims_t{width() / scale().x, _list.height()});
            //_list.set(ListDims_t{width() / scale().x, _list.height()});
        _list.set(Loc(0, _inverted ? -_list.height() : height()));
    }
    
    void Dropdown::set_inverted(bool invert) {
        if(_inverted == invert)
            return;
        
        _inverted = invert;
        if(_inverted)
            _list.set_pos(-Vec2(0, _list.height()));
        else
            _list.set_pos(Vec2(0, height()));

        set((LabelCornerFlags)(CornerFlags)_label_corner_flags);
        set(_list_corner_flags);
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

void Dropdown::on_text_changed() {
    _set_open(true);
    
    if (_custom_on_text_changed) {
        auto text = _textfield->text();
        _custom_on_text_changed(text);
    }

    _items_changed = true;
    apply_item_filtering();
    set_content_changed(true);
}

void Dropdown::apply_item_filtering() {
    if(not _items_changed)
        return;
    
    _filtered_items.clear();
    _items_to_filtered_items.clear();
    
    if(not _textfield)
        return; // abort if we arent initialized fully yet
    
    bool list_changed{false};

    if (not _textfield->text().empty())
    {
        if(_corpus.empty()) {
            _corpus.reserve(items().size());
            
            _doc_corpus.reserve(items().size());
            _doc_corpus.clear();
            
            const bool has_docs = [this](){
                for(auto &item : _items) {
                    if(item.docs()) {
                        return true;
                    }
                }
                return false;
            }();
            
            for (const auto &item : _items) {
                _corpus.push_back(item.search_name()
                        ? item.search_name().value()
                        : item.name());
                
                if(not has_docs)
                    continue;
                
                if(item.docs())
                    _doc_corpus.push_back(item.docs().value());
                else
                    _doc_corpus.push_back("");
            }
        }
        
        if(not _preprocessed) {
            if(_doc_corpus.empty()) {
                _preprocessed = preprocess_corpus(_corpus);
            } else {
                _preprocessed = preprocess_corpus(_corpus, _doc_corpus);
            }
        }
        
        // Filter items using the search text
        // Get the search result indexes
        std::vector<int> search_result_indexes;
        assert(_preprocessed.has_value());
        
        if(std::holds_alternative<PreprocessedData>(_preprocessed.value())) {
            search_result_indexes = text_search(_textfield->text(), _corpus, std::get<PreprocessedData>(_preprocessed.value()));
        } else {
            search_result_indexes = text_search(_textfield->text(), _corpus, _doc_corpus, std::get<PreprocessedDataWithDocs>(_preprocessed.value()));
        }

        // Create a filtered vector of TextItems
        std::vector<TextItem> filtered;
        for (int _index : search_result_indexes) {
            RawIndex index(_index);
            filtered.push_back(_items[_index]);
            _filtered_items[FilteredIndex{static_cast<long>(filtered.size() - 1)}] = index;
            _items_to_filtered_items[index] = FilteredIndex{static_cast<long>(filtered.size() -1)};
        }

        // Update the list with the filtered items
        if(_list.set_items(convert_to_search_name(filtered))) {
            list_changed = true;
            _selected_item = RawIndex();
            if(_list.scroll_enabled())
                _list.set_scroll_offset(Vec2());
        }

    } else {
        // If the search text is empty, display all items
        // trivial mapping
        for(long i=0; i<(long)_items.size(); ++i) {
            _filtered_items[FilteredIndex{i}] = RawIndex{i};
            _items_to_filtered_items[RawIndex{i}] = FilteredIndex{i};
        }
        
        list_changed = _list.set_items(convert_to_search_name(_items));
    }

    if(list_changed) {
        this->select_item(_selected_item);
        this->set_content_changed(true);
    }
    
    _items_changed = false;
}

const Drawable* Dropdown::tooltip_object() const {
    if(_button
       && _button->is_staged()
       && _button->hovered())
    {
        return _button.get();
        
    } else if(_list.is_staged()
              && _list.hovered())
    {
        if(_list.tooltip_object())
            return _list.tooltip_object();
        return &_list;
    }
    
    if(_textfield
       && _textfield->is_staged())
    {
        return _textfield.get();
    }
    
    return nullptr;
}

}
