#include "List.h"
#include <misc/Timer.h>
#include <gui/DrawBase.h>

namespace cmn::gui {

    List::~List() {

    }
    
    List::List(const Bounds& size, const std::string& title, const std::vector<std::shared_ptr<Item>>& items, const std::function<void(List*, const Item&)>& on_click)
    : //gui::DrawableCollection("List"+std::to_string((long)this)),
        _title(Str{title}, TextClr(White), _label_font),
        _title_background(),
        _accent_color(Drawable::accent_color),
        _on_click(on_click),
        _toggle(false),
        _selected_rect(NULL),
        _foldable(true),
        _folded(true),
        _selected_item(-1),
        _multi_select(false),
        _display_selection(false),
        _on_toggle([](){}),
        _row_height(size.height)
    {
        //set_bounds({size.pos(), Size2(size.width, (_folded ?  0 : _row_height * _items.size()) + margin*2 + (title_height+title_margin*3))});
        set_bounds(Bounds(size.pos(), Size2(size.width, _row_height)));
        set_background(Transparent, Black.alpha(255));
        set_items(items);
        set_clickable(true);
        //set_scroll_enabled(true);
        
        _title_background.set_clickable(true);
        _title_background.on_click([this](auto) {
            // toggle folded state by clicking on title
            if(this->foldable())
                this->set_folded(!this->folded());
            stage()->select(this);
        });
        
        add_event_handler(EventType::KEY, (event_handler_t)[this](Event e) -> bool {
            if(e.key.pressed) {
                if(e.key.code == Codes::Return) {
                    Rect *rect = nullptr;
                    size_t i;
                    
                    for(i=0; i<_rects.size(); ++i) {
                        if(_rects.at(i)->hovered()) {
                            if(e.key.code == Codes::Down)
                                rect = _rects.at(i ? (i-1) : (_rects.size()-1)).get();
                            else
                                rect = _rects.at(i < _rects.size()-1 ? (i+1) : 0).get();
                            
                            break;
                        }
                    }
                    
                    if(rect) {
                        this->_on_click(this, *_items.at(i));
                        this->toggle_item(i);
                    }
                    
                    return true;
                }
                else if(e.key.code == Codes::Up || e.key.code == Codes::Down) {
                    Rect *rect = nullptr;
                    for(size_t i=0; i<_rects.size(); ++i) {
                        if(_rects.at(i)->hovered()) {
                            if(e.key.code == Codes::Down)
                                rect = _rects.at(i ? (i-1) : (_rects.size()-1)).get();
                            else
                                rect = _rects.at(i < _rects.size()-1 ? (i+1) : 0).get();
                            
                            break;
                        }
                    }
                    
                    if(!rect && !_rects.empty())
                        rect = _rects.front().get();
                    
                    stage()->do_hover(rect);
                    this->set_content_changed(true);
                    
                    return true;
                }
            }
            
            return false;
        });
    }
    
    void List::set_items(std::vector<std::shared_ptr<Item>> items) {
        for(size_t i=0; i<items.size(); ++i) {
            if(items[i]->ID() == -1)
                items[i]->set_ID(i);
        }
        
        bool clean_list = items.size() == _items.size();
        if(clean_list) {
            for(size_t i=0; i<items.size(); i++) {
                if(!(*items.at(i) == *_items.at(i))) {
                    clean_list = false;
                    break;
                }
            }
        }
        
        if(clean_list)
            return;
        
        DrawStructure::Lock_t *guard = NULL;
        if(stage())
            guard = new GUI_LOCK(stage()->lock());
        
        _items = items;
        _selected_rect = NULL;
        
        std::function<void(const std::shared_ptr<Rect>&, const std::shared_ptr<Item>&)>
        func = [this](const std::shared_ptr<Rect>&, const std::shared_ptr<Item>& item) {
            item->_list = this;
        };
        
        update_vector_elements(_rects, _items, func);
        
        if(!_selected_rect)
            _selected_item = -1;
        
        update_sizes();
        
        if(guard)
            delete guard;
    }

    void List::update_sizes() {
        float max_w = {100};
        Font font{_label_font};
        font.size = max(font.size, _item_font.size);
        
        for(size_t i=0; i<_items.size(); i++) {
            auto &item = _items.at(i);
            auto w = Base::default_text_bounds(std::string(*item), this, font).width;
            if(w > max_w)
                max_w = w;
        }
        
        //max_w = min(width(), max_w);
        _max_w = max_w;
        Entangled::set_size(Size2(_width_limit > 0
                                  ? min(_width_limit, _max_w + 20)
                                  : (_max_w + 20),
                                  height()));
        set_content_changed(true);
    }
    
    void List::select_item(long ID) {
        set_content_changed(true);
        
        if(!_multi_select) {
            for(auto item : _items)
                if(item->ID() != ID && item->selected())
                    item->set_selected(false);
        }
            
        
        for(size_t i=0; i<_items.size(); i++) {
            if(ID == _items.at(i)->ID()) {
                if(_selected_rect != _rects.at(i)) {
                    set_dirty();
                    
                    _selected_rect = _rects.at(i);
                    _selected_item = ID;
                }
                
                return;
            }
        }
        
        Print("Item ",ID," cannot be found.");
    }

void List::on_click(const Item * item) {
    if(_on_click)
        _on_click(this, *item);
}

    void List::toggle_item(long ID) {
        if(!_toggle) {
            set_selected(ID, true);
            return;
        }
        
        if(!_multi_select) {
            for(auto item : _items)
                if(item->ID() != ID && item->selected())
                    item->set_selected(false);
        }
        
        for(size_t i=0; i<_items.size(); i++) {
            if(ID == _items.at(i)->ID()) {
                if(_selected_rect == _rects.at(i)) {
                    _selected_rect = NULL;
                    _selected_item = -1;
                } else {
                    _selected_rect = _rects.at(i);
                    _selected_item = ID;
                }
                
                if(_toggle)
                    _items.at(i)->set_selected(!_items.at(i)->selected());
                else {
                    _items.at(i)->set_selected(true);
                    _items.at(i)->set_selected(false);
                }
                    
                
                set_content_changed(true);
                return;
            }
        }
        
        throw U_EXCEPTION("Item ",ID," cannot be found.");
    }
    
    void List::set_selected(long ID, bool selected) {
        set_content_changed(true);
        
        if(!_multi_select && selected) {
            for(auto item : _items)
                if(item->ID() != ID && item->selected())
                    item->set_selected(false);
        }
        
        for(size_t i=0; i<_items.size(); i++) {
            if(ID == _items.at(i)->ID()) {
                //if(_items.at(i)->selected() == selected)
                //    return;
                
                if(selected) {
                    _selected_rect = _rects.at(i);
                    _selected_item = ID;
                } else if(_selected_item == ID) {
                    _selected_rect = NULL;
                    _selected_item = -1;
                }
                /*if(_selected_rect == _rects.at(i)) {
                    _selected_rect = NULL;
                    _selected_item = -1;
                } else {*/
                
                //}
                
                _items.at(i)->set_selected(selected);
                return;
            }
        }
        
        throw U_EXCEPTION("Item ",ID," cannot be found.");
    }
    
    void List::deselect_all() {
        if(_selected_item != -1)
            set_content_changed(true);
        
        _selected_item = -1;
        _selected_rect = NULL;
        
        for(auto item : _items)
            item->set_selected(false);
    }
    
    void List::draw_title() {
        advance_wrap(_title_background);
        if(!_title.txt().empty())
            advance_wrap(_title);
        else {
            std::string item_name = "-";
            for(auto &item : items()) {
                if(item->ID() == selected_item()) {
                    item_name = (std::string)*item;
                    break;
                }
            }
            add<Text>(Str{item_name}, Loc(_title.pos()), TextClr{_title.color()}, _label_font);
        }
    }
    
    void List::update() {
        Timer timer;
        
        auto size = bounds();
        auto gb = global_bounds();
        
        //if(!folded())
        //Print(stage()->height(), " -> ", (gb.y + _items.size() * _row_height * gscale), "  @ position ", gb," -> ", 
        //    stage()->height() - (gb.y + _items.size() * _row_height * gscale));
        const bool inverted = foldable() && stage() 
            //&& gb.y + (_items.size()) * _row_height * gscale >= stage()->height() / stage()->scale().y
            && gb.y >= stage()->height() / stage()->scale().y * 0.5;
            //&& stage()->height() - (gb.y + _items.size() * _row_height * gscale) < gb.y - (gb.y + _items.size() * _row_height * gscale);
        
        if(foldable()) {
            Entangled::set_size(Size2(width(), _row_height));//(1 + (_folded ? 0 : _items.size())) * _row_height));
        } else
            set_background(Black.alpha(150));
        
        if(scroll_enabled())
            set_scroll_limits(Rangef(0, 0), Rangef(0, max(0, row_height() * (_items.size() + 1) - height())));
        
        _title_background.set_bounds(Bounds(scroll_enabled() ? scroll_offset() : Vec2(), Size2(width(), _row_height)));
        _title.set_pos((scroll_enabled() ? scroll_offset() : Vec2()) + Vec2(10, _row_height * 0.5));
        
        Color tbg = _accent_color;
        if(foldable()) {
            if(pressed()) {
                if(hovered())
                    tbg = tbg.exposure(0.5);
                else
                    tbg = tbg.exposure(0.3);
                
            } else {
                if(_foldable && !folded()) {
                    if(hovered()) {
                        tbg = tbg.exposure(0.7);
                    } else
                        tbg = tbg.exposure(0.5);
                    
                } else if(hovered()) {
                    tbg = tbg.exposure(1.5);
                    tbg.a = saturate(tbg.a * 1.5);
                }
            }
        }
        
        _title_background.set_fillclr(tbg);
        
        const Color bg = _accent_color.saturation(0.25);
        const Color highlight = bg.exposure(1.5);
        
        for(size_t i=0; i<_items.size(); i++) {
            _items.at(i)->update();
            auto r = _rects.at(i);
            
            auto clr = bg;
            if(i%2 == 0)
                clr = clr.exposure(1.25);
            
            clr = r->pressed() ?
                  (r->hovered() ? clr.exposure(0.5) : clr.exposure(0.3))
                : (r->hovered() ? highlight : clr);
            
            
            if((_toggle || _display_selection) && _items.at(i)->selected()) //r == _selected_rect)
                clr = r->hovered() ? Color(25, 220) : Color(0, 220);

            r->set_fillclr(clr);
        }
        
        if(!content_changed())
            return;
        
        begin();
        
        Vec2 offset(Vec2(0, 0));
        float inversion_correct_height = inverted ? -_row_height : _row_height;
        
        //if(!_title.txt().empty())
            offset.y += inversion_correct_height;//title_height + title_margin*2;
        
        if(foldable() && folded()) {
            draw_title();
            end();
            return;
        }
        
        for(size_t i=0; i<_items.size(); i++) {
            auto r = _rects.at(i);
            advance_wrap(*r);
        }
        
        for(size_t i=0; i<_items.size(); i++) {
            auto &item = _items.at(i);
            auto r = _rects.at(i);

            r->set_bounds(Bounds(offset, Vec2(size.width, _row_height)));
            add<Text>(Str(*item),
                      Loc(offset + Vec2(size.width, _row_height)*0.5f),
                      _item_font);
            offset.y += inversion_correct_height;
        }
        
        if(_items.empty()) {
            if(not _placeholder.empty()) {
                auto f = _item_font;
                f.align = Align::Center;
                //Print("size = ", size);
                add<Text>(Str((std::string)_placeholder),
                          Loc(size.size() * 0.5),
                          f, TextClr{LightGray.alpha(200)});
            }
        }
        
        draw_title();
        end();
    }

    void List::set_size(const Size2& _size) {
        auto size = _size;
        if(_size.width != _width_limit)
            _width_limit = _size.width;
        size.width = _width_limit > 0 ? min(_width_limit, width()) : _size.width;
        
        if(size.Equals(_bounds.size()))
            return;
        
        set_row_height(size.height);
        Entangled::set_size(size);
        set_content_changed(true);
    }

    void List::set_bounds(const Bounds& size) {
        set_size(size.size());
        set_pos(size.pos());
    }

void List::set_display_selection(bool v) {
    if(v == _display_selection)
        return;
    
    _display_selection = v;
    set_content_changed(true);
}
void List::set_toggle(bool toggle) {
    if(_toggle == toggle)
        return;
    
    _toggle = toggle;
    set_content_changed(true);
}
void List::set_multi_select(bool s) {
    if(_multi_select == s)
        return;
    
    _multi_select = s;
    set_content_changed(true);
}
void List::set_accent_color(Color color) {
    if(_accent_color == color)
        return;
    
    _accent_color = color;
    set_content_changed(true);
}
void List::set(Placeholder_t placeholder) {
    if(_placeholder == placeholder)
        return;
    
    _placeholder = placeholder;
    set_content_changed(true);
}
void List::set_foldable(bool f) {
    if(_foldable == f)
        return;
    
    if(!f && _folded)
        _folded = false;
    
    _foldable = f;
    set_content_changed(true);
    if(_foldable && not _folded)
        set_z_index(2);
    else
        set_z_index(0);
}
void List::set_folded(bool f) {
    if(_folded == f)
        return;
    
    _folded = f;
    set_content_changed(true);
    if(_foldable && not _folded) {
        set_z_index(2);
        
        if(foldable() && hovered()) {
            if(stage())
                stage()->do_hover(nullptr);
        }
    } else
        set_z_index(0);
    _on_toggle();
}

void List::set_row_height(float v) {
    if(_row_height == v)
        return;
    
    _row_height = v;
    set_content_changed(true);
}

void List::on_toggle(std::function<void()> fn) {
    _on_toggle = fn;
}

void List::set_selected_rect(std::shared_ptr<Rect> r) {
    _selected_rect = r;
}

void List::set_title(const std::string& title) {
    _title.set_txt(title);
}

void List::set(LabelColor_t clr) {
    set_accent_color(clr);
}

void List::set(LabelBorderColor_t clr) {
    set(LineClr{(Color)clr});
}

void List::set(attr::HighlightClr clr) {
    set_accent_color(clr);
    //Entangled::set(clr);
}
void List::set(attr::Str content) {
    set_title(content);
}
void List::set(ItemFont_t font) {
    font.align = Align::Center;
    if(font != _item_font) {
        _item_font = font;
        update_sizes();
        set_content_changed(true);
    }
}
void List::set(LabelFont_t font) {
    if(font.align != Align::Center)
        font.align = Align::VerticalCenter;
    if(font != _label_font) {
        _label_font = font;
        _title.set(_label_font);
        update_sizes();
        set_content_changed(true);
    }
}

}
