#include "Layout.h"
#include <gui/types/SingletonObject.h>

namespace gui {
    Layout::Layout(const std::vector<Layout::Ptr>& objects)
        : _objects(objects)
    {
        set_content_changed(true);
        //set_background(Red.alpha(125));
    }
    
    void Layout::update() {
        if(!content_changed())
            return;
        
        begin();
        for(auto o : _objects)
            advance_wrap(*o);
        end();
        
        update_layout();
        set_content_changed(false);
    }
    
    void Layout::add_child(size_t pos, Layout::Ptr ptr) {
        auto it = std::find(_objects.begin(), _objects.end(), ptr);
        if(it != _objects.end()) {
            if(it == _objects.begin() + pos || it == _objects.begin() + pos - 1)
                return;
            else if(it < _objects.begin() + pos)
                pos--;
            _objects.erase(it);
        }
        
        if (pos < _objects.size())
            _objects.insert(_objects.begin() + pos, ptr);
        else
            _objects.push_back(ptr);
        
        set_content_changed(true);
        update();
    }

    void Layout::add_child(Layout::Ptr ptr) {
        auto it = std::find(_objects.begin(), _objects.end(), ptr);
        if (it != _objects.end()) {
            if (it == _objects.begin() + _objects.size() - 1)
                return;
            _objects.erase(it);
        }

        _objects.insert(_objects.end(), ptr);

        set_content_changed(true);
        update();
    }

    void Layout::update_layout() {
        auto_size({});
    }

    void Layout::set_children(const std::vector<Layout::Ptr>& objects) {
        if(std::set<Layout::Ptr>(objects.begin(), objects.end()).size() != objects.size())
            throw U_EXCEPTION("Cannot insert the same object multiple times.");
        
        std::vector<Layout::Ptr> next;
        bool dirty = false;
        
        for(auto& obj : objects) {
            next.push_back(obj);
            
            auto it = std::find(_objects.begin(), _objects.end(), obj);
            if(it != _objects.end()) {
                continue;
            }
            
            dirty = true;
        }
        
        if(!dirty) {
            for(auto& obj : _objects) {
                auto it = std::find(next.begin(), next.end(), obj);
                if(it == next.end()) {
                    dirty = true;
                    break;
                }
            }
        }
        
        if(dirty) {
            _objects = next;
            set_content_changed(true);
            update();
        }
    }
    
    void Layout::remove_child(Layout::Ptr ptr) {
        auto it = std::find(_objects.begin(), _objects.end(), ptr);
        if(it == _objects.end())
            return;
        _objects.erase(it);
        
        Entangled::remove_child(ptr.get());

        set_content_changed(true);
        update();
    }
    
    void Layout::remove_child(gui::Drawable *ptr) {
        Entangled::remove_child(ptr);

        auto it = std::find(_objects.begin(), _objects.end(), ptr);
        if(it != _objects.end()) {
            set_content_changed(true);
            _objects.erase(it);
            return;
        }
        //print("Cannot find object ",ptr);
    }
    
    /*void Layout::set_children(const std::vector<Layout::Ptr>& objects) {
        if(std::set<Layout::Ptr>(objects.begin(), objects.end()).size() != objects.size())
            throw U_EXCEPTION("Cannot insert the same object multiple times.");
        
        if(_objects == objects)
            return;
        
        clear_children();
        
        _objects = objects;
        set_content_changed(true);
        update();
    }*/
    
    void Layout::clear_children() {
        _objects.clear(); 
        set_content_changed(true);
        Entangled::clear_children(); 
    }
    
    HorizontalLayout::HorizontalLayout(const std::vector<Layout::Ptr>& objects,
                                       const Vec2& position,
                                       const Bounds& margins)
        : gui::Layout(objects), _margins(margins), _policy(CENTER)
    {
        set_pos(position);
        update();
    }
    
    void HorizontalLayout::update_layout() {
        float x = 0;
        float max_height = _margins.y + _margins.height;
        
        for(auto c : children()) {
            if(!c)
                continue;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->update_layout();
            }

            c->update_bounds();
        }
        
        //if(_policy == CENTER || _policy == BOTTOM)
        {
            for(auto c : children()) {
                if(!c)
                    continue;
                max_height = max(max_height, c->local_bounds().height + _margins.height + _margins.y);
            }
        }
        
        for(auto c : children()) {
            if(!c)
                continue;
            
            x += _margins.x;

            auto local = c->local_bounds();
            auto offset = local.size().mul(c->origin());
            
            if(_policy == CENTER)
                c->set_pos(offset + Vec2(x, (max_height - local.height) * 0.5));
            else if(_policy == TOP)
                c->set_pos(offset + Vec2(x, _margins.y));
            else if(_policy == BOTTOM)
                c->set_pos(offset + Vec2(x, max_height - _margins.height - local.height));
            
            x += local.width + _margins.width;
        }
        
        if(not Size2(x, max(0, max_height)).Equals(size())) {
            set_size(Size2(x, max(0, max_height)));
            set_content_changed(true);
        }
    }
    
    void HorizontalLayout::set_policy(Policy policy) {
        if(_policy == policy)
            return;
        
        _policy = policy;
        update_layout();
    }
    
    VerticalLayout::VerticalLayout(const std::vector<Layout::Ptr>& objects,
                                       const Vec2& position,
                                       const Bounds& margins)
        : gui::Layout(objects), _margins(margins), _policy(LEFT)
    {
        set_pos(position);
        update();
    }
    
    void VerticalLayout::update_layout() {
        float y = 0;
        float max_width = _margins.x + _margins.width;
        
        for(auto c : children()) {
            if(!c)
                continue;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->update_layout();
            }

            c->update_bounds();
        }
        
        for(auto c : children()) {
            if(!c)
                continue;
            
            max_width = max(max_width, c->local_bounds().width + _margins.width + _margins.x);
            assert(not std::isnan(max_width));
        }
        
        for(auto c : children()) {
            if(!c)
                continue;
            
            y += _margins.y;
            
            auto local = c->local_bounds();
            auto offset = local.size().mul(c->origin());
            
            if(_policy == CENTER)
                c->set_pos(offset + Vec2((max_width - local.width) * 0.5, y));
            else if(_policy == LEFT)
                c->set_pos(offset + Vec2(_margins.x, y));
            else if(_policy == RIGHT)
                c->set_pos(offset + Vec2(max_width - _margins.width - local.width, y));
            
            y += local.height + _margins.height;
        }
        
        set_size(Size2(max_width, max(0.f, y)));
    }

    void Layout::auto_size(Margin margin) {
        Vec2 mi(std::numeric_limits<Float2_t>::max()), ma(0);
        
        for(auto c : children()) {
            if(!c)
                continue;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->auto_size(margin);
            }

            auto bds = c->local_bounds();
            mi = min(bds.pos(), mi);
            ma = max(bds.pos() + bds.size(), ma);
        }
        
        if(mi.x != std::numeric_limits<Float2_t>::max()) {
            ma += Vec2(max(0.f, margin.right), max(0.f, margin.bottom));
            set_size(ma - mi);
        }
    }
    
    void VerticalLayout::set_policy(Policy policy) {
        if(_policy == policy)
            return;
        
        _policy = policy;
        update_layout();
    }

    void VerticalLayout::set_margins(const Bounds &margins) {
        if(_margins == margins)
            return;
        
        _margins = margins;
        update_layout();
    }

    void HorizontalLayout::set_margins(const Bounds &margins) {
        if(_margins == margins)
            return;
        
        _margins = margins;
        update_layout();
    }

GridLayout::GridLayout(const std::vector<Layout::Ptr>& objects,
                       const Bounds& margins)
    : gui::Layout(objects),
    _margins(margins)
{
    update();
}

void GridLayout::set_margins(const Bounds &) {
    
}

void GridLayout::set_policy(Policy) {
    
}

void GridLayout::update_layout() {
    std::vector<float> max_col_widths, max_row_heights;

    // Step 1: Calculate max dimensions for columns and rows
    for (auto _row : objects()) {
        if(not _row.is<Layout>())
            continue;

        Layout* row = _row.to<Layout>();
        float row_height = 0;
        size_t col_idx = 0;

        for (auto _cell : row->objects()) {
            if(not _cell.is<Layout>())
                continue;

            Layout* cell = _cell.to<Layout>();
            assert(cell->bounds().x < 1000000);
            cell->auto_size({});
            cell->update_layout();  // Recursive update, so it computes its size
            auto cell_bounds = cell->local_bounds();
            assert(cell_bounds.x < 1000000);
            
            // Update max dimensions for row and column
            if (col_idx >= max_col_widths.size()) {
                max_col_widths.push_back(cell_bounds.width);
            } else {
                max_col_widths[col_idx] = std::max(max_col_widths[col_idx], cell_bounds.width);
            }

            row_height = std::max(row_height, cell_bounds.height);
            ++col_idx;
        }

        row->auto_size({});
        max_row_heights.push_back(row_height);
    }
    
    print("max_row_heights = ", max_row_heights);
    print("max_col_widths = ", max_col_widths);

    // Step 2: Update the position of each row and cell, relative to its parent
    float y = _margins.y;
    size_t i = 0;

    for (auto _row : objects()) {
        if(not _row.is<Layout>())
            continue;
        
        Layout* row = _row.to<Layout>();

        // Reset x-coordinate at the beginning of each new row
        float x = _margins.x;

        float row_height = max_row_heights[i];
        size_t col_idx = 0;  // Column index

        for (auto _cell : row->objects()) {
            if(not _cell.is<Layout>())
                continue;

            Layout* cell = _cell.to<Layout>();
            assert(cell->bounds().x < 1000000);
            cell->auto_size({});
            assert(cell->bounds().x < 1000000);
            auto cell_bounds = cell->local_bounds();

            float cell_y = _margins.y + (row_height - cell_bounds.height) * 0.5f;

            // Ensure that cell_x is calculated afresh for each cell based on x and not dependent on old value.
            float cell_x;
            if (_policy == CENTER) {
                cell_x = x + (max_col_widths[col_idx] - cell_bounds.width) * 0.5f;
            } else if (_policy == LEFT) {
                cell_x = x;
            } else if (_policy == RIGHT) {
                cell_x = x + (max_col_widths[col_idx] - cell_bounds.width);
            } else
                cell_x = x;
            
            assert(cell->bounds().x < 1000000);
            std::cout << "Before set_pos - Cell[" << i << "][" << col_idx << "] pos: " << cell->pos().x << ","<< cell->pos().y << "\n";
            cell->set_pos({cell_x, cell_y});
            std::cout << "After set_pos - Cell[" << i << "][" << col_idx << "] pos: " << cell->pos().x << ","<< cell->pos().y << "\n";
            
            assert(cell->bounds().x < 1000000);
            x += max_col_widths[col_idx] + _margins.width;
            ++col_idx;
        }

        //row->auto_size({});
        
        // Update the position of the row itself here if necessary
        // Corrected this part
        row->set_pos({0, y});
        row->set_size({x, row_height + _margins.height + _margins.y});
        print("row ", i, " is at position ", row->pos());
        y += row_height + _margins.height + _margins.y; // Updated to row_height from max_row_heights[col_idx]
        ++i; // Increment row index
    }
}


}
