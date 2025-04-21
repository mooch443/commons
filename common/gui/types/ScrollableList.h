#pragma once

#include <commons.pc.h>
#include <gui/types/Entangled.h>
#include <gui/DrawSFBase.h>
#include <gui/types/Tooltip.h>
#include <gui/DrawStructure.h>
#include <gui/ListAttributes.h>

namespace cmn::gui {
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

    template<typename T>
    concept has_disabled = requires(T t) {
        { t.disabled() } -> std::convertible_to<bool>;
    };

    //! Check compatibility with List class
    template<typename T>
    concept list_compatible_item = requires(T t) {
        requires std::convertible_to<T, std::string>;
        { t != t }; // has != operator
    };

    template <typename T = std::string>
    requires list_compatible_item<T>
    class ScrollableList : public Entangled {
        ItemPadding_t _item_padding;
        ItemFont_t _item_font{ Font(0.75) };
        DetailFont_t _detail_font{ Font(0.55) };
        ListDims_t _list_dims;
        LabelDims_t _label_dims;
        DetailColor_t _detail_color{Gray};
        GETTER(ListFillClr_t, list_fill_clr){100,100,100,200};
        GETTER(ListLineClr_t, list_line_clr){200,200,200,200};
        LabelColor_t _label_fill_clr{100,100,100,200};
        LabelBorderColor_t _label_line_clr{200,200,200,200};
        
        template <typename Q = T>
        class Item {
            GETTER(Q, value);
            GETTER_SETTER_I(bool, hovered, false);
            
        public:
            Item(T v) : _value(v) { }
        };
        
        GETTER(std::vector<Item<T>>, items);
        std::vector<std::shared_ptr<Rect>> _rects;
        std::vector<StaticText*> _texts;
        std::vector<StaticText*> _details;
        std::unique_ptr<StaticText> _label_text;

        StaticText _placeholder_text;
        Tooltip tooltip = Tooltip({}, 300);
        
        std::function<void(size_t, const T&)> _callback;
        OnHover_t _on_hovered;
        
        GETTER(LabelFont_t, label_font);
        GETTER(Color, item_color);
        GETTER(ItemBorderColor_t, item_line_color);
        GETTER_I(Color, text_color, White);
        float _line_spacing, _previous_width{-1};
        GETTER_SETTER_I(long, last_hovered_item, -1);
        GETTER_SETTER_I(bool, foldable, false);
        GETTER_I(bool, folded, false);
        GETTER_I(std::string, folded_label, "");
        GETTER_I(long, last_selected_item, -1);
        GETTER_I(long, currently_highlighted_item, -1);
        GETTER_I(bool, stays_toggled, false);
        GETTER_I(bool, alternating_rows, false);
        
        std::map<Drawable*, size_t> rect_to_idx;
        Entangled _list;
        
    public:
        template<typename... Args>
        ScrollableList(Args... args) {
            _list.set_clickable(true);
            
            create(std::forward<Args>(args)...);
            add_event_handler(SELECT, [this](Event e) {
                if(not e.select.selected && _foldable) {
                    if(stage() && (not stage()->selected_object() || (stage()->selected_object() && not stage()->selected_object()->is_child_of(this))))
                    {
                        set(Folded_t{true});
                    }
                }
            });
            add_event_handler(HOVER, [this](auto) { set_content_changed(true); });
            add_event_handler(MBUTTON, [this](Event e){
                this->set_dirty();
                
                if(!e.mbutton.pressed && e.mbutton.button == 0) {
                    if(_foldable && _folded) {
                        set(Folded_t{false});
                    } else {
                        Entangled * o = _foldable ? &_list : this;
                        float idx;
                        if(_foldable) {
                            idx = (floorf((o->scroll_offset().y - _list.pos().y + e.mbutton.y) / _line_spacing));

                        } else {
                            idx = (floorf((o->scroll_offset().y + e.mbutton.y) / _line_spacing));
                        }
                        if(_foldable) {
                            //if(not _list.bounds().contains(Vec2(e.mbutton.x, e.mbutton.y)))
                            {
                                set(Folded_t{true});
                                //return;
                            }
                        }

                        _last_selected_item = -1;
                        if (idx >= 0 && idx < _items.size()) {
                            select_item(idx);
                        } else
                            select_item(-1);
                    }
                }
            });
            add_event_handler(SCROLL, [this](auto) {
                this->update_items();
            });
            _list.add_event_handler(SCROLL, [this](auto) {
                this->update_items();
            });
            _list.set_z_index(1);
        }
        
        template<typename... Args>
        void create(Args... args) {
            init();
            
            (set(std::forward<Args>(args)), ...);
            
            update_line_height();
            //set_background(_item_color.exposure(0.5));
            update_items();
        }
        
        void init() {
            set(ItemPadding_t{5,5});
            set(ItemColor_t(50,50,50,220));
            set(ItemBorderColor_t(Transparent));
            set(ItemFont_t{ Font(0.75) });
            set(ListDims_t{ 100, 200 });
            set(TextClr{_text_color});
            set(ListFillClr_t{100,100,100,200});
            set(ListLineClr_t{200,200,200,200});
            set(LabelColor_t{100,100,100,200});
            set(LabelBorderColor_t{200,200,200,200});
            update_line_height();
            
            set_clickable(true);
        }
        
        void update_line_height() {
            _line_spacing = [this] (){
                if constexpr(has_detail<T>) {
                    return Base::default_line_spacing(_item_font) * 2 + _item_padding.y * 2;
                }
                return Base::default_line_spacing(_item_font) + _item_padding.y * 2;
            }();
            
        }
        
        ~ScrollableList() {
            //for(auto r : _rects)
            //    delete r;
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
        
        float line_height() const {
            return _line_spacing;
        }
        
        void set(const std::vector<T>& objs) {
            set_items(objs);
        }
        
        void set(FillClr) override {}
        void set(LineClr) override {}
        void set(OnHover_t hover) {
            _on_hovered = hover;
        }
        void set(DetailColor_t clr) {
            if (_detail_color == clr)
                return;
            _detail_color = clr;
            set_content_changed(true);
        }
        void set(ListLineClr_t clr) {
            if (_list_line_clr == clr)
                return;
            _list_line_clr = clr;
            set_content_changed(true);
        }
        void set(ListFillClr_t clr) {
            if (_list_fill_clr == clr)
                return;
            _list_fill_clr = clr;
            set_content_changed(true);
        }
        void set(LabelColor_t clr) {
            if (_label_fill_clr == clr)
                return;
            _label_fill_clr = clr;
            set_content_changed(true);
        }
        void set(LabelBorderColor_t clr) {
            if (_label_line_clr == clr)
                return;
            _label_line_clr = clr;
            set_content_changed(true);
        }
        void set(TextClr lr) {
            set_text_color(lr);
        }
        void set(DetailFont_t font) {
            if(_detail_font == font)
                return;
            
            _detail_font = font;
            
            if constexpr(has_detail<T>) {
                font = detail_font({});
                for(auto d : _details) {
                    if(d)
                        d->set(font);
                }
            }
            
            update_line_height();
        }
        void set(ItemFont_t font) {
            if(_item_font == font)
                return;
            _item_font = font;
            
            update_line_height();
            
            if constexpr(has_detail<T>) {
                for(auto d : _details) {
                    if(d)
                        d->set(detail_font(font));
                }
            }
            
            _placeholder_text.set_default_font(font);
            update_line_height();
        }
        void set(Alternating_t alternating) {
            set_alternating(alternating);
        }
        void set(Foldable_t foldable) {
            if(_foldable == foldable)
                return;
            _foldable = foldable;
            if(_foldable) {
                if(scroll_enabled()) {
                    set_scroll_limits(Rangef(), Rangef());
                    set_scroll_offset(Vec2());
                    set_scroll_enabled(false);
                }
                
            } else {
                set_scroll_enabled(true);
                set_scroll_limits(Rangef(), Rangef());
                set_scroll_offset(Vec2());
            }
            set_content_changed(true);
        }
        void set(Folded_t folded) {
            if(_folded == folded)
                return;
            _folded = folded;
            set_content_changed(true);
        }
        void set(Str text) {
            if(_folded_label == text)
                return;
            _folded_label = text;
            set_content_changed(true);
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
            _currently_highlighted_item = -1;
            _items.clear();

            //Float2_t y = _line_spacing * objs.size();
            Entangled * o = _foldable ? &_list : this;
            //if (y + o->scroll_offset().y < 0)
            {
                if(o->scroll_enabled())
                    o->set_scroll_offset(Vec2());
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
            
            //set_background(item_color.exposure(0.5));
            _item_color = item_color;
            set_dirty();
        }
        void set(const ItemColor_t& item_color) {
			set_item_color(item_color);
		}
        void set(const ItemBorderColor_t& c) {
            if(_item_line_color == c)
                return;
            _item_line_color = c;
            set_content_changed(true);
        }
        
        void set_text_color(const Color& text_color) {
            if(_text_color == text_color)
                return;
            
            _text_color = text_color;
            for(auto t : _texts)
                t->set_text_color(text_color);
            
            uchar c = (int(text_color.r) + int(text_color.g) + int(text_color.b)) / 3 >= 150 ? 200 : 50;
            _placeholder_text.set(TextClr{c, c, c, text_color.a});
            set_content_changed(true);
        }
        
        void set_alternating(bool alternate) {
            if(_alternating_rows == alternate)
                return;
            
            _alternating_rows = alternate;
            set_content_changed(true);
        }
        
        void refresh_dims() {
            update_line_height();
            if(_foldable) {
                _list.set_size(Size2(_list_dims.width, _list_dims.height > 0 ? min(_list_dims.height, _items.size() * _line_spacing) : _items.size() * _line_spacing));
                /*if(_items.size() * _line_spacing <= _list_dims.height)
                    _list.set_scroll_enabled(false);
                else*/
                    _list.set_scroll_enabled(true);
                
                Entangled::set_size(_label_dims);
                    
            } else {
                Entangled::set_size(_list_dims);
                _list.set_size(_list_dims);
                set_scroll_enabled(true);
            }
        }
        
        using Entangled::set;
        void set(const LabelFont_t& font) {
            set_label_font(font);
        }
        void set(const ListDims_t& dims) {
            if(dims == _list_dims)
                return;
            _list_dims = dims;
            refresh_dims();
            set_content_changed(true);
        }
        void set(const LabelDims_t& dims) {
            if(dims == _label_dims)
                return;
            
            _label_dims = dims;
            set_content_changed(true);
        }
        void set(const Placeholder_t& placeholder) {
            if(_placeholder_text.text() == placeholder)
                return;
            
            _placeholder_text.set_txt(placeholder);
            if(_items.empty())
                set_content_changed(true);
        }
        
        void set_label_font(const LabelFont_t& font) {
            if(_label_font == font)
                return;
            
            _label_font = font;
            
            // line spacing may have changed
            if(_label_font.size != font.size
               || _label_font.style != font.style)
            {
                set_bounds_changed();
            }
            
            update_items();
        }
        
        void set(const ItemPadding_t& padding) {
            set_item_padding(padding);
        }
        
        void set_item_padding(const ItemPadding_t& padding) {
            if(padding == _item_padding)
                return;
            
            _item_padding = padding;
            update_line_height();
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
            for(auto &r : _rects) {
                if(r->hovered()) {
                    size_t idx = rect_to_idx.count(r.get()) ? rect_to_idx.at(r.get()) : 0;
                    return idx;
                }
            }
        }
        
        void highlight_item(long index) {
            Entangled * o = _foldable ? &_list : this;
            float first_visible = o->scroll_offset().y / _line_spacing;
            float last_visible = (o->scroll_offset().y + height()) / _line_spacing;
            
            if(index == -1)
                return;
            
            if(o->scroll_enabled()) {
                if(index > last_visible-1) {
                    float fit = index - o->height() / _line_spacing + 1;
                    o->set_scroll_offset(Vec2(0, fit * _line_spacing));
                }
                else if(index < first_visible)
                    o->set_scroll_offset(Vec2(0, _line_spacing * index));
            }
            
            update_items();
            update();
            
            first_visible = floorf(o->scroll_offset().y / _line_spacing);
            last_visible = min(_items.size()-1.0f, floorf((o->scroll_offset().y + o->height()) / _line_spacing));
            _last_hovered_item = index;
            _currently_highlighted_item = index;
            
            //draw_structure()->do_hover(_rects.at(index - first_visible));
            
            if(index >= first_visible
               && index <= last_visible
               && stage())
            {
                    stage()->do_hover(_rects.at(sign_cast<size_t>(index - first_visible)).get());
            }
        }
        
        void select_item(uint64_t index) {
            if(_items.size() > index
               && static_cast<uint64_t>(_last_selected_item) != index)
            {
                if constexpr(has_disabled<T>) {
                    if(_items.at(index).value().disabled())
                        return;
                }
                
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
            Entangled * e = _foldable ? &_list : this;
            
            size_t N = size_t(ceilf(max(0.f, e->height()) / _line_spacing)) + 1u; // one item will almost always be half-visible
            
            if(N != _rects.size()) {
                if(N < _rects.size()) {
                    for(size_t i=N; i<_rects.size(); i++) {
                        //delete _rects.at(i);
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
                        if(_details.size() > N) {
                            _details.erase(_details.begin() + int64_t(N), _details.end());
                        }
                    }
                }
                
                ItemFont_t item_font = _item_font;
                item_font.align = Align::Left;
                Font detail_font = this->detail_font(item_font);
                
                for(size_t i=0; i<_rects.size(); i++) {
                    _rects.at(i)->set_size(Size2(_list_dims.width - 2, item_height - 1));
                    _texts.at(i)->set_default_font(item_font);
                    if constexpr(has_detail<T>) {
                        if(_details.size() > i)
                            _details.at(i)->set(detail_font);
                    }
                }
                
                for(size_t i=_rects.size(); i<N; i++) {
                    _rects.push_back(std::make_shared<Rect>(Box(0, 0, width(), item_height), i%2 == 0 ? FillClr{Transparent} : FillClr{Blue.alpha(150)}));
                    _rects.back()->on_hover([_r = std::weak_ptr(_rects.back()), this](Event e)
                    {
                        if(auto r = _r.lock();
                           r && rect_to_idx.count(r.get()))
                        {
                            auto idx = rect_to_idx.at(r.get());
                            if(!e.hover.hovered) {
                                if(_currently_highlighted_item == (long)idx) {
                                    _currently_highlighted_item = -1;
                                }
                                return;
                            } else {
                                if(_last_hovered_item != (long)idx) {
                                    _last_hovered_item = long(idx);
                                    _currently_highlighted_item = long(idx);
                                    if(_on_hovered)
                                        _on_hovered(idx);
                                }
                            }
                        }
                    });
                    
                    _texts.push_back(new StaticText(attr::Font(item_font), attr::Margins(0,0,0,0), attr::TextClr{_text_color}));
                    if constexpr(has_detail<T>) {
                        _details.push_back(new StaticText(detail_font, attr::TextClr{ (Color)_detail_color }, attr::Margins{0}));
                    }
                }
            }
            
            for(auto &rect : _rects) {
                rect->set_size(Size2(_list_dims.width, item_height));
            }
            
            //set_content_changed(true);
        }
        
        /*void update_bounds() override {
            if(!bounds_changed())
                return;
            
            Entangled::update_bounds();
         
        }*/
        
    public:
        void set_bounds(const Bounds& bounds) override {
            set(LabelDims_t{bounds.size()});
            
            if(not pos().Equals(bounds.pos())) {
                Entangled::set_pos(bounds.pos());
            }
        }
        
        void set_size(const Size2& size) override {
            set(LabelDims_t{size});
        }
        
    private:
        Font detail_font(ItemFont_t) const {
            //font.size *= 0.7;
            //return Font(font);
            auto copy = _detail_font;
            //copy.align = _item_font.align;
            return copy;
        }
        
        void update() override {
            if(_foldable && not _folded) {
            }
            
            auto border = _item_line_color;
            if(content_changed()) {
                refresh_dims();
                
                if(width() != _previous_width) {
                    _previous_width = width();
                    if(_foldable && _list.scroll_enabled())
                        _list.set_scroll_offset(Vec2());
                    
                    update_items();
                }
                
                if(_foldable) {
                    _list.set(FillClr{(Color)_list_fill_clr});
                    _list.set(LineClr{(Color)_list_line_clr});
                    reset_bg();
                    
                    //_list.set_background(_list_fill_clr, _list_line_clr);
                    //set_background(Transparent, Transparent);
                    
                } else {
                    Entangled::set(FillClr{(Color)_list_fill_clr});
                    Entangled::set(LineClr{(Color)_list_line_clr});
                    _list.reset_bg();
                    
                    //_list.set_background(Transparent, Transparent);
                    //set_background(_list_fill_clr, _list_line_clr);
                }

                const float item_height = _line_spacing;
                auto color = pressed() ? 
                          _label_fill_clr.exposureHSL(0.5)
                        : ((_foldable && not _folded)
                            ? _label_fill_clr.exposureHSL(0.75)
                            : (not hovered()
                                ? _label_fill_clr : _label_fill_clr.exposureHSL(1.25)));
                
                if(not _label_text)
                    _label_text = std::make_unique<StaticText>();
                _label_text->create(Str{_folded_label}, Loc{
                    _label_dims.width * (_label_font.align == Align::Left
                        ? 0.0_F
                        : (_label_font.align == Align::Center ? 0.5_F : 1.0_F)),
                    _label_dims.height * 0.5_F
                }, Str{_folded_label}, Font(_label_font.size), Origin{
                    _label_font.align == Align::Left
                        ? 0.0_F
                        : (_label_font.align == Align::Center ? 0.5_F : 1.0_F),
                    0.5
                });
                
                if(_foldable && _folded) {
                    auto ctx = OpenContext();
                    add<Rect>(Box{0.f, 0.f, _label_dims.width, _label_dims.height}, FillClr{ (Color)color }, LineClr{ (Color)_label_line_clr });
                    advance_wrap(*_label_text);
                    
                } else {
                    Entangled * e = _foldable ? &_list : this;
                    if(_foldable)
                        _list.set_z_index(2);
                    else
                        _list.set_z_index(0);
                    
                    {
                        auto ctx = e->OpenContext();
                        
                        size_t first_visible = (size_t)max(0.f, floorf(e->scroll_offset().y / item_height));
                        size_t last_visible = (size_t)max(0.f, floorf((e->scroll_offset().y + e->height()) / item_height));
                        
                        rect_to_idx.clear();
                        
                        for(size_t i=first_visible, idx = 0; i<=last_visible && i<_items.size() && idx < _rects.size(); i++, idx++) {
                            auto& item = _items[i];
                            const float y = i * item_height;
                            _rects.at(idx)->set_pos(Vec2(1, y + 1));
                            if constexpr(has_disabled<T>) {
                                _rects.at(idx)->set_clickable(not item.value().disabled());
                            } else
                                _rects.at(idx)->set_clickable(true);
                            
                            _texts.at(idx)->set_txt(item.value());
                            if constexpr(has_detail<T>) {
                                _details.at(idx)->set_txt(item.value().detail());
                            }
                            
                            if constexpr (has_color_function<T>) {
                                if (item.value().color() != Transparent)
                                    _texts.at(idx)->set_text_color(Color::blend(_text_color.alpha(130), item.value().color().alpha(125)));
                                else
                                    _texts.at(idx)->set_text_color(_text_color);
                                
                            } else if constexpr(has_disabled<T>) {
                                if(item.value().disabled())
                                    _texts.at(idx)->set_text_color(_text_color.exposureHSL(0.5).alpha(50));
                                else
                                    _texts.at(idx)->set_text_color(_text_color);
                                
                                if constexpr(has_detail<T>) {
                                    if(item.value().disabled())
                                        _details.at(idx)->set(TextClr{ (Color)_detail_color.alpha(max(10,_detail_color.a * 0.75))});
                                    else
                                        _details.at(idx)->set(TextClr{ (Color)_detail_color });
                                }
                            }
                            
                            if constexpr (has_font_function<T>) {
                                if (item.value().font().size > 0) {
                                    _texts.at(idx)->set_default_font(item.value().font());
                                    if constexpr(has_detail<T>) {
                                        _details.at(idx)->set(detail_font(item.value().font()));
                                    }
                                }
                            } else if constexpr(has_detail<T>) {
                                _details.at(idx)->set(detail_font(_item_font));
                            }
                            
                            rect_to_idx[_rects.at(idx).get()] = i;
                            
                            e->advance_wrap(*_rects.at(idx));
                            e->advance_wrap(*_texts.at(idx));
                            if constexpr(has_detail<T>) {
                                e->advance_wrap(*_details.at(idx));
                            }
                            
                            if constexpr(has_detail<T>) {
                                // Set max sizes
                                _texts.at(idx)->set_max_size(Size2(-1, item_height * 0.5f));
                                _details.at(idx)->set_max_size(Size2(-1, item_height * 0.5f));

                                // Compute heights
                                float text_height = _texts.at(idx)->height();
                                float detail_height = _details.at(idx)->height();
                                float total_content_height = text_height + detail_height;

                                // Compute vertical start position
                                float ystart = y + (item_height - total_content_height) * 0.5f;

                                if (_item_font.align == Align::Center) {
                                    // Center alignment
                                    _texts.at(idx)->set_origin(Vec2{0.5_F, 0_F});
                                    _details.at(idx)->set_origin(Vec2{0.5_F, 0_F});

                                    float x_center = width() * 0.5f;

                                    _texts.at(idx)->set_pos(Vec2{x_center, ystart});
                                    _details.at(idx)->set_pos(Vec2{x_center, ystart + text_height});

                                } else if (_item_font.align == Align::Left) {
                                    // Left alignment
                                    _texts.at(idx)->set_origin(Vec2{0_F, 0_F});
                                    _details.at(idx)->set_origin(Vec2{0_F, 0_F});

                                    float x_left = _item_padding.x;

                                    _texts.at(idx)->set_pos(Vec2{x_left, ystart});
                                    _details.at(idx)->set_pos(Vec2{x_left, ystart + text_height});

                                } else if (_item_font.align == Align::Right) {
                                    // Right alignment
                                    _texts.at(idx)->set_origin(Vec2{1_F, 0_F});
                                    _details.at(idx)->set_origin(Vec2{1_F, 0_F});

                                    float x_right = width() - _item_padding.x;

                                    _texts.at(idx)->set_pos(Vec2{x_right, ystart});
                                    _details.at(idx)->set_pos(Vec2{x_right, ystart + text_height});

                                } else if (_item_font.align == Align::VerticalCenter) {
                                    // Vertical center alignment (assuming left horizontal alignment)
                                    _texts.at(idx)->set_origin(Vec2{0_F, 0_F});
                                    _details.at(idx)->set_origin(Vec2{0_F, 0_F});

                                    float x_left = _item_padding.x;

                                    _texts.at(idx)->set_pos(Vec2{x_left, ystart});
                                    _details.at(idx)->set_pos(Vec2{x_left, ystart + text_height});
                                }
                            } else {
                                // Items without details
                                _texts.at(idx)->set_max_size(Size2(-1, item_height));

                                float text_height = _texts.at(idx)->height();
                                float ystart = y + (item_height - text_height) * 0.5f;

                                if (_item_font.align == Align::Center) {
                                    _texts.at(idx)->set_origin(Vec2{0.5_F, 0_F});
                                    float x_center = width() * 0.5f;
                                    _texts.at(idx)->set_pos(Vec2{x_center, ystart});

                                } else if (_item_font.align == Align::Left) {
                                    _texts.at(idx)->set_origin(Vec2{0_F, 0_F});
                                    float x_left = _item_padding.x;
                                    _texts.at(idx)->set_pos(Vec2{x_left, ystart});

                                } else if (_item_font.align == Align::Right) {
                                    _texts.at(idx)->set_origin(Vec2{1_F, 0_F});
                                    float x_right = width() - _item_padding.x;
                                    _texts.at(idx)->set_pos(Vec2{x_right, ystart});

                                } else if (_item_font.align == Align::VerticalCenter) {
                                    _texts.at(idx)->set_origin(Vec2{0_F, 0_F});
                                    float x_left = _item_padding.x;
                                    _texts.at(idx)->set_pos(Vec2{x_left, ystart});
                                }
                            }

                        }
                        
                        if(_items.empty()) {
                            if(not _placeholder_text.text().empty()) {
                                _placeholder_text.set(Origin{0.5});
                                _placeholder_text.set(Loc{width() * 0.5f, height() * 0.5f});
                                e->advance_wrap(_placeholder_text);
                            }
                        }
                    }
                    
                    if(e->scroll_enabled()) {
                        const float last_y = item_height * (_items.size()-1);
                        e->set_scroll_limits(Rangef(),
                                             Rangef(0,
                                                    (e->height() < last_y ? last_y + item_height - e->height() : 0.f)));
                        auto scroll = e->scroll_offset();
                        e->set_scroll_offset(Vec2());
                        e->set_scroll_offset(scroll);
                    }
                    
                    if(_foldable) {
                        auto ctx = OpenContext();
                        add<Rect>(Box{0.f, 0.f, _label_dims.width, _label_dims.height}, FillClr{ (Color)color }, LineClr{ (Color)_label_line_clr });
                        
                        advance_wrap(*_label_text);
                        advance_wrap(_list);
                        
                        float x = width() - _list.width();
                        auto dims = stage()->dialog_window_size();
                        if(global_bounds().x + (global_bounds().width - _list.global_bounds().width) < 0) {
                            x = -pos().x;
                        } else if(global_bounds().x - x >= dims.width) {
                            Transform transform = global_transform().getInverse();
                            auto rect = transform.transformRect(Bounds(Vec2(), dims));
                            x = rect.width + rect.x - _list.width();
                        }
                        
                        if(global_bounds().y - _list.global_bounds().height < 0) {
                            _list.set_pos(Vec2(x, height()));
                        } else
                            _list.set_pos(Vec2(x, -_list.height()));
                    }
                }
            }

            Color base_color;
            for (auto &rect : _rects) {
                if (not rect_to_idx.contains(rect.get()))
                    continue;
                auto idx = rect_to_idx[rect.get()];
                _items[idx].set_hovered(rect->hovered());

                if constexpr (has_tooltip<T>) {
                    auto tt = _items[idx].value().tooltip();
                    if (rect->hovered()) {
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
                        base_color = base_color.exposure(1.5);
                }

                if (rect->pressed() || (_stays_toggled && (long)rect_to_idx[rect.get()] == _last_selected_item))
                    rect->set_fillclr(base_color.exposure(0.15f));
                else if (rect->hovered())
                    rect->set_fillclr(base_color.exposure(1.25f));
                else
                    rect->set_fillclr(base_color.alpha(50));
                
                rect->set_lineclr(border);
            }

            if (stage()) {
                if(!tooltip.text().text().empty() && hovered())
                    stage()->register_end_object(tooltip);
                else {
                    stage()->unregister_end_object(tooltip);
                }
            }
        }
    };
}

