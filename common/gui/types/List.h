#pragma once

#include <gui/types/Entangled.h>
//#include <gui/types/Basic.h>
#include <gui/GuiTypes.h>
#include <gui/DrawStructure.h>
#include <gui/ListAttributes.h>
#include <gui/types/ListItemTypes.h>

namespace gui {
    class List : public Entangled {
    protected:
        gui::Text _title;
        gui::Rect _title_background;
        Font _item_font{0.6, Align::Center};
        
        Color _accent_color;
        
        GETTER(std::vector<std::shared_ptr<Item>>, items);
        std::vector<std::shared_ptr<Rect>> _rects;
        std::function<void(List*, const Item&)> _on_click;
        bool _toggle;
        std::shared_ptr<Rect> _selected_rect;
        
        GETTER(bool, foldable);
        GETTER(bool, folded);
        GETTER(long, selected_item);
        GETTER(bool, multi_select);
        GETTER(bool, display_selection);// display visually, which item has been selected last (independently of toggle);
        std::function<void()> _on_toggle;
        
        GETTER(float, row_height);
        
    public:
        List(const Bounds& size, const std::string& title, const std::vector<std::shared_ptr<Item>>& items, const std::function<void(List*, const Item&)>& on_click = [](List*, const Item&){});
        virtual ~List() override;
        
        using Entangled::set;
        void set(attr::HighlightClr clr);
        void set(attr::Str content);
        void set(Font font);
        void set(LabelColor_t);
        void set(LabelBorderColor_t);
        void set_size(const Size2&) override;
        void set_bounds(const Bounds&) override;
        
        void set_display_selection(bool v);
        void set_toggle(bool toggle);
        void set_multi_select(bool s);
        void set_accent_color(Color color);
        void set_foldable(bool f);
        void set_folded(bool f);
        
        void set_row_height(float v);
        void on_toggle(std::function<void()> fn);
        void set_items(std::vector<std::shared_ptr<Item>> items);
        void set_title(const std::string& title);
        void select_item(long ID);
        void set_selected(long ID, bool selected);
        void toggle_item(long ID);
        void deselect_all();
        
        void on_click(const Item*);
        void set_selected_rect(std::shared_ptr<Rect>);
        
        void update() override;
    private:
        void draw_title();
    };
}
