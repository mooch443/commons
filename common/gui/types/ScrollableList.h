#pragma once

#include <gui/types/Entangled.h>
#include <gui/DrawSFBase.h>
#include <misc/checked_casts.h>
#include <gui/types/Tooltip.h>

namespace gui {
    /*
     * Helper concepts (C++20) to determine 
     * whether a given Item type has color / font information attached.
     */

    template<typename T>
    concept has_font_function = requires(T t) {
        { t.font() } -> std::convertible_to<Font>;
    };

    template<typename T> 
    concept has_color_function = requires(T t) { 
        { t.color() } -> std::convertible_to<Color>;
    };

    template<typename T>
    concept has_base_color_function = requires(T t) {
        { t.base_color() } -> std::convertible_to<Color>;
    };

    template<typename T>
    concept has_tooltip = requires(T t) {
        { t.tooltip() } -> std::convertible_to<std::string>;
    };

    template<typename T>
    concept has_detail = requires(T t) {
        { t.detail() } -> std::convertible_to<std::string>;
    };

    //! Check compatibility with List class
    template<typename T>
    concept list_compatible_item = requires(T t) {
        requires std::convertible_to<T, std::string>;
        { t != t }; // has != operator
    };
    
    ATTRIBUTE_ALIAS(Placeholder_t, std::string)
    ATTRIBUTE_ALIAS(OnHover_t, std::function<void(size_t)>)

    template <typename T = std::string>
    requires list_compatible_item<T>
    class ScrollableList : public Entangled {
        Vec2 item_padding;
        
        template <typename Q = T>
        class Item {
            GETTER(Q, value)
            GETTER_SETTER_I(bool, hovered, false)
            
        public:
            Item(T v) : _value(v) { }
        };
        
        GETTER(std::vector<Item<T>>, items)
        std::vector<Rect*> _rects;
        std::vector<StaticText*> _texts;
        std::vector<Text*> _details;

        StaticText _placeholder_text;
        Tooltip tooltip = Tooltip(nullptr);
        
        std::function<void(size_t, const T&)> _callback;
        OnHover_t _on_hovered;
        
        GETTER(Font, font)
        GETTER(Color, item_color)
        GETTER_I(Color, text_color, White)
        float _line_spacing, _previous_width{-1};
        GETTER_SETTER_I(long, last_hovered_item, -1)
        GETTER_I(long, last_selected_item, -1)
        GETTER_I(bool, stays_toggled, false)
        GETTER_I(bool, alternating_rows, false)
        
        std::map<Drawable*, size_t> rect_to_idx;
        
    public:
        template<typename... Args>
        ScrollableList(Args... args) {
            create(std::forward<Args>(args)...);
            
            add_event_handler(HOVER, [this](auto){ this->set_dirty(); });
            add_event_handler(MBUTTON, [this](Event e){
                this->set_dirty();
                
                if(!e.mbutton.pressed && e.mbutton.button == 0) {
                    size_t idx = size_t(floorf((scroll_offset().y + e.mbutton.y) / _line_spacing));
                    _last_selected_item = -1;
                    select_item(idx);
                }
            });
            add_event_handler(SCROLL, [this](auto) {
                this->update_items();
            });
        }
        
        template<typename... Args>
        void create(Args... args) {
            init();
            
            (set(std::forward<Args>(args)), ...);
            
            _line_spacing = Base::default_line_spacing(_font) + item_padding.y * 2;
            set_background(_item_color.exposure(0.5));
            update_items();
        }
        
        void init() {
            item_padding = {5,5};
            _item_color = Color(100, 100, 100, 200);
            set(TextClr{_text_color});
            
            set_clickable(true);
            set_scroll_enabled(true);
        }
        
        ~ScrollableList() {
            for(auto r : _rects)
                delete r;
            for(auto t : _texts)
                delete t;
            
            if constexpr(has_detail<T>) {
                for(auto d : _details) {
                    if(d)
                        delete d;
                }
            }
            
            if (stage())
                stage()->unregister_end_object(tooltip);
        }
        
        void set_stays_toggled(bool v) {
            if(v == _stays_toggled)
                return;
            
            _stays_toggled = v;
            update_items();
        }
        
        float line_padding() const {
            return _line_spacing;
        }
        
        void set(const std::vector<T>& objs) {
            set_items(objs);
        }
        
        void set(OnHover_t hover) {
            _on_hovered = hover;
        }
        void set(TextClr lr) {
            set_text_color(lr);
        }
        
        size_t set_items(const std::vector<T>& objs) {
            if(_items.size() == objs.size()) {
                bool okay = true;
                
                for(size_t i=0; i<_items.size(); ++i) {
                    if(_items.at(i).value() != objs.at(i)) {
                        okay = false;
                        break;
                    }
                }
                
                if(okay)
                    return 0u;
            }
            
            _last_selected_item = -1;
            _last_hovered_item = -1;
            _items.clear();

            Float2_t y = _line_spacing * objs.size();
            if (y + scroll_offset().y < 0) {
                set_scroll_offset(Vec2());
            }

            for (auto& item : objs) {
                _items.push_back(Item<T>(item));
            }
            
            set_content_changed(true);
            return _items.size();
        }
        
        void set_item_color(const Color& item_color) {
            if(_item_color == item_color)
                return;
            
            set_background(item_color.exposure(0.5));
            _item_color = item_color;
            set_dirty();
        }
        
        void set_text_color(const Color& text_color) {
            if(_text_color == text_color)
                return;
            
            _text_color = text_color;
            for(auto t : _texts)
                t->set_text_color(text_color);
            
            uchar c = (int(text_color.r) + int(text_color.g) + int(text_color.b)) / 3 >= 150 ? 200 : 50;
            _placeholder_text.set(TextClr{c, c, c, text_color.a});
        }
        
        void set_alternating(bool alternate) {
            if(_alternating_rows == alternate)
                return;
            
            _alternating_rows = alternate;
            set_content_changed(true);
        }
        
        using Entangled::set;
        void set(const Font& font) {
            set_font(font);
        }
        void set(const Placeholder_t& placeholder) {
            if(_placeholder_text.text() == placeholder)
                return;
            
            _placeholder_text.set_txt(placeholder);
            if(_items.empty())
                set_content_changed(true);
        }
        
        void set_font(const Font& font) {
            if(_font == font)
                return;
            
            _line_spacing = [this, font] (){
                if constexpr(has_detail<T>) {
                    return Base::default_line_spacing(font) * 2 + item_padding.y * 2;
                }
                return Base::default_line_spacing(font) + item_padding.y * 2;
            }();
            
            for(auto t : _texts)
                t->set_default_font(font);
            
            if constexpr(has_detail<T>) {
                auto detail_font = font;
                detail_font.size *= 0.75;
                
                for(auto d : _details) {
                    if(d)
                        d->set_font(detail_font);
                }
            }
            
            _placeholder_text.set_default_font(font);
            
            // line spacing may have changed
            if(_font.size != font.size || _font.style != font.style) {
                set_bounds_changed();
            }
            
            _font = font;
            update_items();
        }
        
        void set_item_padding(const Vec2& padding) {
            if(padding == item_padding)
                return;
            
            item_padding = padding;
            _line_spacing = [this] (){
                if constexpr(has_detail<T>) {
                    return Base::default_line_spacing(_font) * 2 + item_padding.y * 2;
                }
                return Base::default_line_spacing(_font) + item_padding.y * 2;
            }();
            update_items();
        }
        
        /**
         * Sets the callback function for when an item is selected.
         * @parm fn function that gets an item index (size_t) and a handle to the item (const T&)
         */
        void on_select(const std::function<void(size_t, const T&)>& fn) {
            _callback = fn;
        }
        
        size_t highlighted_item() const {
            for(auto r : _rects) {
                if(r->hovered()) {
                    size_t idx = rect_to_idx.count(r) ? rect_to_idx.at(r) : 0;
                    return idx;
                }
            }
        }
        
        void highlight_item(long index) {
            float first_visible = scroll_offset().y / _line_spacing;
            float last_visible = (scroll_offset().y + height()) / _line_spacing;
            
            if(index == -1)
                return;
            
            if(index > last_visible-1) {
                float fit = index - height() / _line_spacing + 1;
                set_scroll_offset(Vec2(0, fit * _line_spacing));
            }
            else if(index < first_visible)
                set_scroll_offset(Vec2(0, _line_spacing * index));
            
            update_items();
            update();
            
            first_visible = floorf(scroll_offset().y / _line_spacing);
            last_visible = min(_items.size()-1.0f, floorf((scroll_offset().y + height()) / _line_spacing));
            _last_hovered_item = index;
            
            //draw_structure()->do_hover(_rects.at(index - first_visible));
            
            if(index >= first_visible && index <= last_visible) {
                if(stage())
                    stage()->do_hover(_rects.at(sign_cast<size_t>(index - first_visible)));
            }
        }
        
        void select_item(uint64_t index) {
            if(_items.size() > index && _last_selected_item != index) {
                _last_selected_item = narrow_cast<long>(index);
                set_content_changed(true);
                
                if(_callback)
                    _callback(index, _items.at(index).value());
            }
        }
        
        void select_highlighted_item() {
            if(!stage())
                return;
            
            if(_last_hovered_item >= 0)
                select_item((uint64_t)_last_hovered_item);
        }
        
    private:
        void update_items() {
            const float item_height = _line_spacing;
            size_t N = size_t(ceilf(max(0.f, height()) / _line_spacing)) + 1u; // one item will almost always be half-visible
            
            if(N != _rects.size()) {
                if(N < _rects.size()) {
                    for(size_t i=N; i<_rects.size(); i++) {
                        delete _rects.at(i);
                        delete _texts.at(i);
                        
                        if constexpr(has_detail<T>) {
                            if(_details.size() > i && _details.at(i)) {
                                delete _details.at(i);
                                _details.at(i) = nullptr;
                            }
                        }
                    }
                    
                    _rects.erase(_rects.begin() + int64_t(N), _rects.end());
                    _texts.erase(_texts.begin() + int64_t(N), _texts.end());
                    
                    if constexpr(has_detail<T>) {
                        if(_details.size() > int64_t(N)) {
                            _details.erase(_details.begin() + int64_t(N), _details.end());
                        }
                    }
                }
                
                auto detail_font = this->detail_font(font());
                for(size_t i=0; i<_rects.size(); i++) {
                    _rects.at(i)->set_size(Size2(width(), item_height));
                    _texts.at(i)->set_default_font(_font);
                    if constexpr(has_detail<T>) {
                        if(_details.size() > i)
                            _details.at(i)->set_font(detail_font);
                    }
                }
                
                for(size_t i=_rects.size(); i<N; i++) {
                    _rects.push_back(new Rect(Box(0, 0, width(), item_height), i%2 == 0 ? FillClr{Transparent} : FillClr{Blue.alpha(150)}));
                    _rects.back()->set_clickable(true);
                    _rects.back()->on_hover([r = _rects.back(), this](Event e) {
                        if(!e.hover.hovered)
                            return;
                        if(rect_to_idx.count(r)) {
                            auto idx = rect_to_idx.at(r);
                            if(_last_hovered_item != (long)idx) {
                                _last_hovered_item = long(idx);
                                if(_on_hovered)
                                    _on_hovered(idx);
                            }
                        }
                    });
                    
                    _texts.push_back(new StaticText(attr::Font(_font), attr::Margins(0,0,0,0), attr::TextClr{_text_color}));
                    if constexpr(has_detail<T>) {
                        _details.push_back(new Text(detail_font, attr::TextClr{Gray}));
                    }
                }
            }
            
            for(auto &rect : _rects) {
                rect->set_size(Size2(width(), item_height));
            }
            
            set_content_changed(true);
        }
        
        /*void update_bounds() override {
            if(!bounds_changed())
                return;
            
            Entangled::update_bounds();
         
        }*/
        
    public:
        void set_bounds(const Bounds& bounds) override {
            if(bounds.size() != size())
                set_content_changed(true);
            Entangled::set_bounds(bounds);
        }
        
        void set_size(const Size2& bounds) override {
            if(size() != bounds)
                set_content_changed(true);
            Entangled::set_size(bounds);
        }
        
    private:
        Font detail_font(Font font) const {
            font.size *= 0.7;
            return font;
        }
        
        void update() override {
            if(content_changed()) {
                const float spacing = [this] (){
                    if constexpr(has_detail<T>) {
                        return Base::default_line_spacing(_font) * 2 + item_padding.y * 2;
                    }
                    return Base::default_line_spacing(_font) + item_padding.y * 2;
                }();
                
                if(spacing != _line_spacing || width() != _previous_width) {
                    _line_spacing = spacing;
                    _previous_width = width();
                    update_items();
                }
                
                const float item_height = _line_spacing;
                begin();
                
                size_t first_visible = (size_t)floorf(scroll_offset().y / item_height);
                size_t last_visible = (size_t)floorf((scroll_offset().y + height()) / item_height);
                                
                rect_to_idx.clear();
                
                for(size_t i=first_visible, idx = 0; i<=last_visible && i<_items.size() && idx < _rects.size(); i++, idx++) {
                    auto& item = _items[i];
                    const float y = i * item_height;
                    _rects.at(idx)->set_pos(Vec2(0, y));
                    
                    _texts.at(idx)->set_txt(item.value());
                    if constexpr(has_detail<T>) {
                        _details.at(idx)->set_txt(item.value().detail());
                    }
                    
                    if constexpr (has_color_function<T>) {
                        if (item.value().color() != Transparent)
                            _texts.at(idx)->set_text_color(Color::blend(_text_color.alpha(130), item.value().color().alpha(125)));
                        else
                            _texts.at(idx)->set_text_color(_text_color);
                    }

                    if constexpr (has_font_function<T>) {
                        if (item.value().font().size > 0) {
                            _texts.at(idx)->set_default_font(item.value().font());
                            if constexpr(has_detail<T>) {
                                _details.at(idx)->set_font(detail_font(item.value().font()));
                            }
                        }
                    } else if constexpr(has_detail<T>) {
                        _details.at(idx)->set_font(detail_font(font()));
                    }
                    
                    rect_to_idx[_rects.at(idx)] = i;
                    
                    advance_wrap(*_rects.at(idx));
                    advance_wrap(*_texts.at(idx));
                    if constexpr(has_detail<T>) {
                        advance_wrap(*_details.at(idx));
                    }
                    
                    if constexpr(has_detail<T>) {
                        if(_font.align == Align::Center) {
                            float ycenter = y + item_height * 0.5;
                            float middle = (_texts.at(idx)->height() - _details.at(idx)->height()) * 0.5;
                            //print(y, "Text ", _texts.at(idx)->text(), " -> ycenter=", ycenter, " item_padding=",item_padding, " height=",_texts.at(idx)->height(), " detail=", _details.at(idx)->height(), " line=",_line_spacing);
                            
                            _texts.at(idx)->set_pos(Vec2{
                                (width() - _texts.at(idx)->width()) * 0.5f, 
                                ycenter - _texts.at(idx)->height() + middle
                            });
                            
                            _details.at(idx)->set_pos(Vec2{
                                roundf(width() * 0.5f),
                                ycenter + _details.at(idx)->height() - middle
                            });
                            
                        } else if(_font.align == Align::Left) {
                            _texts.at(idx)->set_pos(Vec2(0, y) + item_padding);
                            _details.at(idx)->set_pos(Vec2(0, y + _line_spacing * 0.5) + item_padding);
                        } else {
                            _texts.at(idx)->set_pos(Vec2(width() - item_padding.x, y + item_padding.y * 0.5));
                            _details.at(idx)->set_pos(Vec2(width() - item_padding.x, y + item_padding.y));
                        }
                    } else {
                        if(_font.align == Align::Center)
                            _texts.at(idx)->set_pos(Vec2(width() * 0.5f, y + item_height*0.5f));
                        else if(_font.align == Align::Left)
                            _texts.at(idx)->set_pos(Vec2(0, y) + item_padding);
                        else
                            _texts.at(idx)->set_pos(Vec2(width() - item_padding.x, y + item_padding.y));
                    }
                }
                
                if(_items.empty()) {
                    if(not _placeholder_text.text().empty()) {
                        advance_wrap(_placeholder_text);
                    }
                }
                
                end();
            
                const float last_y = item_height * (_items.size()-1);
                set_scroll_limits(Rangef(),
                                  Rangef(0,
                                         (height() < last_y ? last_y + item_height - height() : 0.1f)));
                auto scroll = scroll_offset();
                set_scroll_offset(Vec2());
                set_scroll_offset(scroll);
            }

            Color base_color;
            for (auto rect : _rects) {
                if (not rect_to_idx.contains(rect))
                    continue;
                auto idx = rect_to_idx[rect];
                _items[idx].set_hovered(rect->hovered());

                if constexpr (has_tooltip<T>) {
                    auto tt = _items[idx].value().tooltip();
                    if (rect->hovered() ) {
                        tooltip.set_text(tt);
                        tooltip.set_other(rect);
                        tooltip.set_scale(Vec2(rect->global_bounds().width / rect->width()));
                    }
                }

                if constexpr (has_base_color_function<T>)
                    base_color = _items[idx].value().base_color();
                else
                    base_color = _item_color;
                
                if(_alternating_rows) {
                    if(idx% 2 == 0)
                        base_color = base_color.exposureHSL(1.5);
                }

                if (rect->pressed() || (_stays_toggled && (long)rect_to_idx[rect] == _last_selected_item))
                    rect->set_fillclr(base_color.exposure(0.15f));
                else if (rect->hovered())
                    rect->set_fillclr(base_color.exposure(1.25f));
                else
                    rect->set_fillclr(base_color.alpha(50));
            }

            if (stage()) {
                if(!tooltip.text().text().empty())
                    stage()->register_end_object(tooltip);
                else {
                    stage()->unregister_end_object(tooltip);
                }
            }
        }
    };
}

