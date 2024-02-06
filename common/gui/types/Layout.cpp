#include "Layout.h"
#include <gui/types/SingletonObject.h>
#include <gui/GuiTypes.h>

namespace gui {
    void Layout::init() {
        set_content_changed(true);
        //set_background(Red.alpha(125));
        if(not _objects.empty())
            update();
    }

    void Layout::set_parent(SectionInterface *parent) {
        if(parent != this->parent()) {
            Entangled::set_parent(parent);
            set_content_changed(true);
        }
    }

void Layout::set_content_changed(bool changed) {
    Entangled::set_content_changed(changed);
}
    
    void Layout::update() {
        if(!content_changed())
            return;
        
        begin();
        for(auto& o : _objects) {
            assert(o != nullptr);
            advance_wrap(*o);
        }
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

    void Layout::replace_child(size_t pos, Layout::Ptr ptr) {
        auto it = std::find(_objects.begin(), _objects.end(), ptr);
        if(it != _objects.end()) {
            size_t index = std::distance(_objects.begin(), it);
            if(index != pos) {
                throw InvalidArgumentException("Cannot add ", ptr.get(), " twice (already at ", index, " != ", pos, ")");
            }
        }
        
        if (pos < _objects.size())
            _objects[pos] = ptr;
        else
            throw OutOfRangeException("Cannot add ", ptr.get(), " at ", pos, " which is out of bounds.");
        
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
        //if(name() != "Entangled")
        //    print(name(), " updating.");
        //auto_size();
    }

    void Layout::set_children(std::vector<Layout::Ptr>&& objects) {
#ifndef NDEBUG
        if(std::set<Layout::Ptr>(objects.begin(), objects.end()).size() != objects.size())
            throw U_EXCEPTION("Cannot insert the same object multiple times.");
#endif
        
        std::vector<Layout::Ptr> next;
        bool dirty = false;
        
        for(auto&& obj : objects) {
            auto it = std::find(_objects.begin(), _objects.end(), obj);
            next.emplace_back(std::move(obj));
            
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
            _objects = std::move(next);
            set_content_changed(true);
            update();
        }
        
        objects.clear();
    }

    void Layout::set_children(const std::vector<Layout::Ptr>& objects) {
        std::vector<Layout::Ptr> copy(objects);
        set_children(std::move(copy));
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
    
    void Layout::set_margins(const attr::Margins &margins) {
        if(_margins == margins)
            return;
        
        _margins = margins;
        update_layout();
        set_content_changed(true);
    }
    
    void Layout::clear_children() {
        _objects.clear(); 
        set_content_changed(true);
        Entangled::clear_children(); 
    }

    void Layout::_apply_to_children(Drawable* ptr, const std::function<void (Drawable *)> & fn) {
        if (PlaceinLayout* placein = dynamic_cast<PlaceinLayout*>(ptr)) {
            //print("Placein with ", placein->children().size(), " children.");
            for (auto child : placein->children())
                _apply_to_children(child, fn);
            
        } else {
            fn(ptr);
        }
    }

    void Layout::apply_to_children(const std::function<void (Drawable *)> & fn) {
        for (auto c : children()) {
            if(c)
                _apply_to_children(c, fn);
        }
    }
    
    void HorizontalLayout::init()
    {
        update();
    }

void HorizontalLayout::auto_size() {
    // do nothing
}
void VerticalLayout::auto_size() {
    // do nothing
}
    
    void HorizontalLayout::update_layout() {
        float x = 0;
        float max_height = _margins.y + _margins.height;
        
        // Fetch all children, recursively expanding PlaceinLayout objects
        //std::vector<Drawable*> allChildren = fetch_children();
        
        apply_to_children([&](Drawable* c) {
            if(!c)
                return;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->update_layout();
            }

            _c->update_bounds();
            
            max_height = max(max_height, c->local_bounds().height + _margins.height + _margins.y);
        });
        
        apply_to_children([&](Drawable* c) {
            if(!c)
                return;
            
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
        });
        
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
    
    void VerticalLayout::init()
    {
        update();
    }
    
    void VerticalLayout::update_layout() {
        float y = 0;
        float max_width = _margins.x + _margins.width;
        
        apply_to_children([&](Drawable* c) {
            if(!c)
                return;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->update_layout();
            }

            _c->update_bounds();
            
            max_width = max(max_width, _c->local_bounds().width + _margins.width + _margins.x);
            assert(not std::isnan(max_width));
        });
        
        apply_to_children([&](Drawable* c) {
            if(!c)
                return;
            
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
        });
        
        if(not Size2(max_width, max(0.f, y)).Equals(size())) {
            set_size(Size2(max_width, max(0.f, y)));
            set_content_changed(true);
        }
    }

    void Layout::auto_size() {
        Vec2 mi(std::numeric_limits<Float2_t>::max()), ma(0);
        
        apply_to_children([&](Drawable* c){
            if(!c)
                return;

            auto _c = c;
            if (c->type() == Type::SINGLETON)
                _c = static_cast<const SingletonObject*>(_c)->ptr();

            if (_c->type() == Type::ENTANGLED
                && dynamic_cast<Layout*>(_c))
            {
                static_cast<Layout*>(_c)->update();
            }

            auto bds = _c->local_bounds();
            mi = min(bds.pos(), mi);
            ma = max(bds.pos() + bds.size(), ma);
        });
        
        if(mi.x != std::numeric_limits<Float2_t>::max()) {
            //ma += Vec2(max(0.f, _margins.width + _margins.x), max(0.f, _margins.height + _margins.y));
            set_size(ma);
        } else {
            set_size(Size2());
        }
    }
    
    void VerticalLayout::set_policy(Policy policy) {
        if(_policy == policy)
            return;
        
        _policy = policy;
        update_layout();
    }

template<typename Lambda>
void process_layout(const std::vector<Layout::Ptr> &_objects, Lambda&& lambda) {
    // Row index
    std::size_t row_idx = 0;
    
    // Iterate through each row in objects
    for (const Layout::Ptr &_row : _objects) {
        // Skip if nullptr
        if (!_row) continue;
        // Check the type of _row, and continue to the next iteration if it's not Layout
        if (!_row.is<Layout>()) continue;

        // Convert to confirmed type
        auto row = _row.to<Layout>();

        // Column index
        std::size_t col_idx = 0;

        // Iterate through each cell in the row's objects
        for (const auto &_cell : row->objects()) {
            // Skip if nullptr
            if (!_cell) continue;

            // Check the type of _cell, and continue to the next iteration if it's not Layout
            if (!_cell.is<Layout>()) continue;

            // Convert to confirmed type
            auto cell = _cell.to<Layout>();

            // Call the provided lambda function for processing
            lambda(row_idx, col_idx, cell);

            // Increment the column index
            col_idx++;
        }
        // Increment the row index
        row_idx++;
    }
}

// Function to compute the transform between two Drawable objects
Transform computeRelativeTransform(Drawable* from, Drawable* to) {
    if (!from || !to) {
        // Handle null pointers (perhaps return an identity transform or throw an exception)
        throw std::invalid_argument("Nullptr in computeRelativeTransform");
    }

    // Get the global transforms of both objects
    Transform fromGlobal = from->global_transform();
    Transform toGlobal = to->global_transform();

    // Compute the inverse of the 'from' object's global transform
    Transform fromGlobalInverse = fromGlobal.getInverse();

    // Compute the relative transform
    Transform relativeTransform = fromGlobalInverse * toGlobal;
    return relativeTransform;
}

std::string GridInfo::toStr() const {
    return "Grid<" + Meta::toStr(numCols) + "x" + Meta::toStr(numRows) + " bounds=" + Meta::toStr(gridBounds) + ">";
}

void GridLayout::init() {
    on_hover([this](Event e){
        if(immediately_clickable()) {
            _vertical_rect->set(FillClr{(Color)_settings.verticalClr});
            _horizontal_rect->set(FillClr{(Color)_settings.horizontalClr});
            _last_hover = Vec2(e.hover.x, e.hover.y);
            //print("hovered ", name(), " at ", _last_hover, " with ", e.hover.hovered, " ", (uint64_t)this);
            update_hover();
            set_content_changed(true);
        }
    });
    
    update();
    update_layout();
}

void GridLayout::set(GridLayout::HPolicy policy) {
    if(policy != _settings.hpolicy) {
        _settings.hpolicy = policy;
        set_content_changed(true);
    }
}
void GridLayout::set(GridLayout::VPolicy policy) {
    if(policy != _settings.vpolicy) {
        _settings.vpolicy = policy;
        set_content_changed(true);
    }
}

void GridLayout::set(attr::VerticalClr verticalClr) {
    if(verticalClr != _settings.verticalClr) {
        _settings.verticalClr = verticalClr;
        _vertical_rect->set(FillClr{(Color)verticalClr});
        set_content_changed(true);
    }
}

void GridLayout::set(attr::HorizontalClr horizontalClr) {
    if(horizontalClr != _settings.horizontalClr) {
        _settings.horizontalClr = horizontalClr;
        _horizontal_rect->set(FillClr{(Color)horizontalClr});
        set_content_changed(true);
    }
}

void GridLayout::set(const std::vector<Layout::Ptr>& objects) {
    // Assuming _settings.objects is a std::vector<Layout::Ptr>
    set_children(objects);
}

void GridLayout::update_hover() {
    try {
        for(size_t row = 0; row < _grid_info.numRows; ++row) {
            for(size_t col = 0; col < _grid_info.numCols; ++col) {
                if(_grid_info.hasCell(row, col)) {
                    auto bds = _grid_info.getCellBounds(row, col);
                    if(bds.contains(_last_hover)) {
                        for(size_t r = 0; r < _grid_info.numRows; ++r) {
                            if(r != row)
                                bds.combine(_grid_info.getCellBounds(r, col));
                        }
                        _vertical_rect->set(Box{bds});
                        
                        bds = _grid_info.getCellBounds(row, col);
                        for(size_t c = 0; c < _grid_info.numCols; ++c) {
                            if(c != col)
                                bds.combine(_grid_info.getCellBounds(row, c));
                        }
                        _horizontal_rect->set(Box{bds});
                        break;
                    }
                }
            }
        }
    } catch(const std::out_of_range& ex) {
        // invalid row or column index
        FormatExcept(ex.what());
    }
}

void GridLayout::update_layout() {
    std::vector<float> max_col_widths, max_row_heights;
    GridInfo::GridBounds gridBounds;

    // Step 1: Calculate max dimensions for columns and rows
    for (auto &_row : objects()) {
        if(not _row.is<Layout>())
            continue;

        Layout* row = _row.to<Layout>();
        float row_height = 0;
        size_t col_idx = 0;

        for (auto _cell : row->objects()) {
            if(not _cell.is<Layout>())
                continue;

            Layout* cell = _cell.to<Layout>();
            cell->auto_size();
            
            auto cell_bounds = cell->local_bounds();
            
            // Update max dimensions for row and column
            if (col_idx >= max_col_widths.size()) {
                max_col_widths.push_back(cell_bounds.width);
            } else {
                max_col_widths[col_idx] = std::max(max_col_widths[col_idx], cell_bounds.width);
            }

            row_height = std::max(row_height, cell_bounds.height);
            ++col_idx;
        }
        
        max_row_heights.push_back(row_height);
    }

    // Step 2: Update the position of each row and cell, relative to its parent
    float y = 0;
    size_t row_idx = 0;

    for (auto &_row : objects()) {
        if(not _row.is<Layout>())
            continue;
        
        std::vector<Bounds> rowBounds; // To store the bounds of each cell in the current row
        Layout* row = _row.to<Layout>();

        // Reset x-coordinate at the beginning of each new row
        float x = 0;

        float row_height = max_row_heights[row_idx];
        if(row_height > 0)
            row_height += margins().height + margins().y;
        size_t col_idx = 0;  // Column index

        for (auto &_cell : row->objects()) {
            if(not _cell.is<Layout>())
                continue;

            Layout* cell = _cell.to<Layout>();
            cell->auto_size();
            
            auto cell_bounds = cell->local_bounds();

            float cell_y = _margins.y;
            if(_settings.vpolicy == VPolicy::CENTER) {
                cell_y = (row_height - cell_bounds.height) * 0.5f;
            } else if(_settings.vpolicy == VPolicy::BOTTOM) {
                cell_y = row_height - cell_bounds.height - _margins.height;
            }
            
            // Add this cell's bounds to rowBounds
            rowBounds.push_back({
                x, y,
                max_col_widths[col_idx] + margins().width + margins().x,
                row_height
            });
            
            // Ensure that cell_x is calculated afresh for each cell based on x and not dependent on old value.
            x += _margins.x;
            float cell_x = x;
            if (_settings.hpolicy == HPolicy::CENTER) {
                cell_x = x + (max_col_widths[col_idx] - cell_bounds.width) * 0.5f;
            } else if (_settings.hpolicy == HPolicy::RIGHT) {
                cell_x = x + (max_col_widths[col_idx] - cell_bounds.width);
            }
            
            cell->set_pos({cell_x, cell_y});
            
            //print("cell(",col_idx,",",row_idx,") coordinates ", cell, " -> row_height=", row_height, " @ ", cell->pos()," with margins=", _margins, " and size=", cell->size(), " vs rowbounds=", rowBounds.back());
            /*for(auto c : cell->children()) {
                if(c)
                    print("\t",*c, " ", c->pos());
            }*/

            x += max_col_widths[col_idx] + margins().width;
            ++col_idx;
        }

        // Update the position of the row itself here if necessary
        // Corrected this part
        //row->auto_size();
        row->set_pos({0, y});
        row->set_size({x, row_height});
        
        // Add this row's bounds to gridBounds
        gridBounds.push_back(rowBounds);
        
        y += row->height(); // Updated to row_height from max_row_heights[col_idx]
        ++row_idx; // Increment row index
    }
    
    if(not max_col_widths.empty()) {
        set_size({
            std::accumulate(max_col_widths.begin(), max_col_widths.end(), 0.0f) + (margins().width + margins().x) * max_col_widths.size(),
            y
        });
    }
    
    _grid_info = GridInfo(gridBounds);
    update_hover();
}

void GridLayout::set_parent(SectionInterface * p) {
    set_content_changed(true);
    Layout::set_parent(p);
    update();
}

void GridLayout::update() {
    if(!content_changed())
        return;
    
    begin();
    if(immediately_clickable() && hovered()) {
        advance_wrap(*_vertical_rect);
        advance_wrap(*_horizontal_rect);
    }
    for(auto o : _objects)
        advance_wrap(*o);
    end();
    
    update_layout();
    set_content_changed(false);
}

void GridLayout::children_rect_changed() {
    Layout::children_rect_changed();
}

void GridLayout::auto_size() {
    // nothing?
}


}
