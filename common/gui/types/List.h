#pragma once

#include <gui/types/Entangled.h>
//#include <gui/types/Basic.h>
#include <gui/GuiTypes.h>
#include <gui/DrawStructure.h>
#include <gui/ListAttributes.h>
#include <gui/types/ListItemTypes.h>

namespace cmn::gui {
    class List : public Entangled {
    protected:
        Font _item_font{0.6, Align::Center};
        Font _label_font{0.6, Align::VerticalCenter};
        gui::Text _title{_label_font};
        gui::Rect _title_background;
        Placeholder_t _placeholder;
        CornerFlags_t _corners;
        LabelCornerFlags _label_corners;
        
        Color _accent_color{Drawable::accent_color};
        float _max_w{0};
        float _width_limit{0};
        LabelAutoSize_t _auto_limit_size{true};
        ListFillClr_t _list_fill_clr;
        ListLineClr_t _list_line_clr;
        
        GETTER(std::vector<std::shared_ptr<Item>>, items);
        std::vector<std::shared_ptr<Rect>> _rects;
        std::function<void(List*, const Item&)> _on_click{[](List*, const Item&){}};
        bool _toggle{false};
        std::shared_ptr<Rect> _selected_rect;
        
        GETTER_I(bool, foldable, false);
        GETTER_I(bool, folded, false);
        GETTER_I(long, selected_item, -1);
        GETTER_I(bool, multi_select, false);
        GETTER_I(bool, display_selection, false);// display visually, which item has been selected last (independently of toggle);
        std::function<void()> _on_toggle{[](){}};
        
        GETTER_I(float, row_height, 0);
        GETTER(Vec2, last_list_offset);
        
    public:
        template<typename... Args>
            requires (detail::HasSet<List, Args> && ...)
        List(Args... args)
        {
            init();
            create(std::forward<Args>(args)...);
        }

        template<typename... Args>
            requires (detail::HasSet<List, Args> && ...)
        void create(Args... args) {
            if constexpr(not contains_type<Box, Args...>()
                         && not contains_type<Loc, Args...>())
                set(Loc{0, 0});
            if constexpr(not contains_type<Box, Args...>()
                         && not contains_type<Size, Args...>()
                         && not contains_type<LabelDims_t, Args...>())
                set(Size{100, 28});
            if constexpr(not contains_type<Clickable, Args...>())
                set(Clickable{true});
            if constexpr(not contains_type<Str, Args...>())
                set(Str{});
            if constexpr(not contains_type<ItemFont_t, Args...>())
                set(ItemFont_t{Font{0.6, Align::Center}});
            if constexpr(not contains_type<LabelFont_t, Args...>())
                set(LabelFont_t{Font{0.6, Align::VerticalCenter}});
            if constexpr(not contains_type<Placeholder_t, Args...>())
                set(Placeholder_t{});
            if constexpr(not contains_type<CornerFlags_t, Args...>())
                set(CornerFlags_t{});
            if constexpr(not contains_type<LabelCornerFlags, Args...>())
                set(LabelCornerFlags{});
            if constexpr(not contains_type<ListFillClr_t, Args...>())
                set(ListFillClr_t{Drawable::accent_color.saturation(0.25)});
            if constexpr(not contains_type<ListLineClr_t, Args...>())
                set(ListLineClr_t{Transparent});
            if constexpr(not contains_type<Foldable_t, Args...>())
                set(Foldable_t{false});
            if constexpr(not contains_type<Folded_t, Args...>())
                set(Folded_t{false});
            if constexpr(not contains_type<LabelAutoSize_t, Args...>())
                set(LabelAutoSize_t{true});
            
            (set(std::forward<Args>(args)), ...);
            update_sizes();
        }

        virtual ~List() override;
        
        using Entangled::set;
        void set(std::vector<std::shared_ptr<Item>> items) {
            set_items(std::move(items));
        }
        void set(std::function<void(List*, const Item&)> on_click) {
            _on_click = std::move(on_click);
        }
        void set(Foldable_t foldable) { set_foldable(foldable); }
        void set(Folded_t folded) { set_folded(folded); }
        void set(attr::HighlightClr clr);
        void set(attr::Str content);
        void set(ItemFont_t font);
        void set(LabelFont_t font);
        void set(LabelFillClr_t);
        void set(LabelColor_t);
        void set(ListFillClr_t);
        void set(ListLineClr_t);
        void set(LabelLineColor_t);
        void set_size(const Size2&) override;
        void set_bounds(const Bounds&) override;
        void set(Placeholder_t);
        void set(CornerFlags_t) override;
        void set(LabelCornerFlags);
        void set(const LabelDims_t& dims) {
            set_size(Size2(dims));
        }
        void set(LabelAutoSize_t);
        
        void set_display_selection(bool v);
        void set_toggle(bool toggle);
        void set_multi_select(bool s);
        void set_accent_color(Color color);
        void set_foldable(bool f);
        void set_folded(bool f);
        
        void set_row_height(float v);
        void on_toggle(std::function<void()> fn);
        void set_items(std::vector<std::shared_ptr<Item>>&& items);
        void set_title(const std::string& title);
        void select_item(long ID);
        void set_selected(long ID, bool selected);
        void toggle_item(long ID);
        void deselect_all();
        
        void on_click(const Item*);
        void set_selected_rect(std::shared_ptr<Rect>);
        
        void update() override;
        
        Drawable* tooltip_object() override;
    private:
        void init();
        void draw_title();
        void update_sizes();
    };
}
