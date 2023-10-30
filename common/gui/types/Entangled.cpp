#include "Entangled.h"
#include <gui/DrawStructure.h>
#include <gui/types/Dropdown.h>
#include <misc/stacktrace.h>

namespace gui {
    Entangled::Entangled(Entangled&& other) noexcept
        : SectionInterface(std::move(other)), // Assuming SectionInterface has a move constructor
          _current_children(std::move(other._current_children)),
          _new_children(std::move(other._new_children)),
          _currently_removed(std::move(other._currently_removed)),
          _owned(std::move(other._owned)),
          _begun(std::move(other._begun.load())),
          scrolling(std::move(other.scrolling)),
          callback_ptr(std::exchange(other.callback_ptr, nullptr)),
          _scroll_offset(std::move(other._scroll_offset)),
          _scroll_enabled(std::move(other._scroll_enabled)),
          _scroll_limit_x(std::move(other._scroll_limit_x)),
          _scroll_limit_y(std::move(other._scroll_limit_y)),
          _index(std::move(other._index)),
          _content_changed(std::move(other._content_changed)),
          _content_changed_while_updating(std::move(other._content_changed_while_updating))
    {}

    Entangled& Entangled::operator=(Entangled&& other) noexcept {
        if (this != &other) {
            // Move base part
            SectionInterface::operator=(std::move(other)); // Assuming SectionInterface has a move assignment

            _current_children = std::move(other._current_children);
            _new_children = std::move(other._new_children);
            _currently_removed = std::move(other._currently_removed);
            _owned = std::move(other._owned);
            _begun.store(std::move(other._begun.load()));
            scrolling = std::move(other.scrolling);
            callback_ptr = std::exchange(other.callback_ptr, nullptr);
            _scroll_offset = std::move(other._scroll_offset);
            _scroll_enabled = std::move(other._scroll_enabled);
            _scroll_limit_x = std::move(other._scroll_limit_x);
            _scroll_limit_y = std::move(other._scroll_limit_y);
            _index = std::move(other._index);
            _content_changed = std::move(other._content_changed);
            _content_changed_while_updating = std::move(other._content_changed_while_updating);
        }
        return *this;
    }

    Entangled::Entangled()
        : SectionInterface(Type::ENTANGLED, NULL)
    {}

    Entangled::Entangled(const Bounds& bounds) 
        : SectionInterface(Type::ENTANGLED, NULL)
    {
        set_bounds(bounds);
    }

    Entangled::Entangled(std::function<void(Entangled&)>&& fn)
        : SectionInterface(Type::ENTANGLED, NULL)
    {
        update(std::move(fn));
        auto_size(Margin{0,0});
    }
        
    Entangled::Entangled(const std::vector<Drawable*>& objects)
        : SectionInterface(Type::ENTANGLED, NULL),
            _current_children(objects)
    {
        for(size_t i=0; i<_current_children.size(); i++)
            init_child(_current_children[i], i, true);
    }
    
    void Entangled::update(const std::function<void(Entangled& base)> create) {
        begin();
        create(*this);
        end();
    }
    
    Entangled::~Entangled() {
        auto children = this->children();
        _current_children.clear();
        _new_children.clear();
        
        for(size_t i=0; i<children.size(); i++) {
            if(children[i]) {
                if (stage() && stage()->selected_object() == children[i])
                    stage()->select(NULL);
                if (stage() && stage()->hovered_object() == children[i])
                    stage()->clear_hover();
            
                children[i]->set_parent(NULL);
            }
            //deinit_child(false, children[i]);
        }
    }
    
    void Entangled::set_scroll_enabled(bool enable) {
        if(_scroll_enabled == enable)
            return;
        
        if(callback_ptr) {
            remove_event_handler(SCROLL, callback_ptr);
            callback_ptr = nullptr;
        }
        
        if(enable)
            callback_ptr = add_event_handler(SCROLL, scrolling);
        _scroll_enabled = enable;
        
        clear_cache();
        if(background())
            background()->clear_cache();
        
        children_rect_changed();
    }
    
    void Entangled::set_scroll_limits(const cmn::Rangef &x, const cmn::Rangef &y)
    {
        if(_scroll_limit_x == x && _scroll_limit_y == y)
            return;
        
        _scroll_limit_x = x;
        _scroll_limit_y = y;
        
        children_rect_changed();
    }
    
    /*Drawable* Entangled::entangle(Drawable* d) {
        _set_child(d, true, _index);
        //_children.push_back(d);
        init_child(d, _new_children.size()-1, true);
        
        return d;
    }*/
    
    void Entangled::set_scroll_offset(Vec2 scroll_offset) {
        if(!_scroll_enabled)
            throw U_EXCEPTION("Cannot set scroll offset for object that doesnt have scrolling enabled.");
        
        //if(_scroll_limit_x != Rangef())
        {
            scroll_offset.x = saturate(scroll_offset.x,
                                       _scroll_limit_x.start,
                                       _scroll_limit_x.end);
        }
        
        //if(_scroll_limit_y != Rangef())
        {
            scroll_offset.y = saturate(scroll_offset.y,
                                       _scroll_limit_y.start,
                                       _scroll_limit_y.end);
        }
        
        if(scroll_offset == _scroll_offset)
            return;
        
        _scroll_offset = scroll_offset;
        children_rect_changed();
    }

std::vector<Drawable*>& Entangled::children() {
    if(_begun) {
        //FormatExcept("Accessing children() while iterating.");
        return _new_children;
    }
    return _current_children;
}

bool Entangled::empty() const {
    if(_begun)
        return _new_children.empty();
    //    FormatWarning("Accessing children() while iterating.");
    return _current_children.empty();
}
    
void Entangled::_set_child(Drawable* ptr, bool , size_t index) {
    if(index < _new_children.size()) {
        _new_children[index] = ptr;
    } else {
        _new_children.resize(index + 1);
        _new_children[index] = ptr;
    }
}

void Entangled::on_visibility_change(bool visible) {
    SectionInterface::set_rendered(visible);
    for(auto ptr : children()) {
        if(ptr)
            ptr->set_rendered(visible);
    }
}

    void Entangled::begin() {
        if(_begun) {
            print_stacktrace();
            throw U_EXCEPTION("Cannot begin twice.");
        }
        
        _begun = true;
        _index = 0;
        _currently_removed.clear();
        //std::fill(_current_children.begin(), _current_children.end(), nullptr);
        //_new_children.resize(_current_children.size());
        //std::fill(_new_children.begin(), _new_children.end(), nullptr);
        _new_children.clear();
    }
    void Entangled::end() {
        _begun = false;
        
        for(size_t i=_index; i < _current_children.size(); ++i) {
            auto tmp = _current_children[i];
            if(tmp) {
                tmp->set_parent(NULL);
                auto it = _owned.find(tmp);
                if(it != _owned.end()) {
                    if(it->second)
                        delete tmp;
                    _owned.erase(it);
                }
                _current_children[i] = nullptr;
            }
        }
        
        while(!_currently_removed.empty()) {
            auto d = *_currently_removed.begin();
            if (!d) {
                _currently_removed.erase(_currently_removed.begin());
                continue;
            }
            d->set_parent(NULL);
            
            if(!_currently_removed.empty() && *_currently_removed.begin() == d) {
/*#ifndef NDEBUG
                FormatWarning("Had to deinit forcefully");
#endif*/
                if(d->parent()) {
                    d->set_parent(nullptr);
                }
                
                auto it = _owned.find(d);
                if(it != _owned.end()) {
                    if(it->second)
                        delete d;
                    _owned.erase(it);
                }
                _currently_removed.erase(_currently_removed.begin());
            }
        }
        
        _new_children.resize(_index);
        std::swap(_current_children, _new_children);
            //deinit_child(false, *_currently_removed.begin());
        
#ifndef NDEBUG
        for(auto c : _current_children) {
            assert(c);
        }
#endif
    }
    
    void Entangled::auto_size(Margin margin) {
        if(children().empty()) {
            set_size(Size2());
            return;
        }
        
        Vec2 mi(std::numeric_limits<Float2_t>::max()), ma(0);
        
        for(auto c : children()) {
            if(!c)
                continue;
            
            auto bds = c->local_bounds();
            mi = min(bds.pos(), mi);
            ma = max(bds.pos() + bds.size(), ma);
        }
        
        ma += Vec2(max(0.f, margin.right), max(0.f, margin.bottom));
        
        set_size(ma - mi);
    }
    
    void Entangled::update_bounds() {
        if(!bounds_changed())
            return;
        
        SectionInterface::update_bounds();
    }
    
    void Entangled::set_content_changed(bool c) {
        if(_content_changed == c && _content_changed_while_updating == c)
            return;
        
        _content_changed = c;
        _content_changed_while_updating = true;
        
        if(c)
            set_dirty();
        
        if(c)
            SectionInterface::set_bounds_changed();
    }
    
    void Entangled::before_draw() {
        _content_changed_while_updating = false;
        update();
        
        for(auto c : children()) {
            if(!c)
                continue;
            
            if(c->type() == Type::ENTANGLED)
                static_cast<Entangled*>(c)->before_draw();
        }
        
        if(_content_changed && !_content_changed_while_updating)
            _content_changed = false;
        else if(_content_changed_while_updating)
            set_dirty();
    }
    
    void Entangled::set_parent(SectionInterface* p) {
        //children_rect_changed();
        if(p != _parent) {
            if(p)
                set_content_changed(true);
            SectionInterface::set_parent(p);
        }
    }
    
    void Entangled::children_rect_changed() {
        //if(scroll_enabled())
        //    set_bounds_changed();
        //else
        SectionInterface::children_rect_changed();
    }
    
    bool Entangled::swap_with(Drawable*) {
        // TODO: ownership
        throw U_EXCEPTION("Ownership not implemented. You need to save Entangled objects and use wrap_object instead of add_object.");
    }
    
    void Entangled::remove_child(Drawable* d) {
        //if(_begun)
        //    throw U_EXCEPTION("Undefined while updating.");
        
        //if(_begun)
        //    FormatWarning("Deleting child while updating parent.");
        
        auto oit = _owned.find(d);
        if(oit != _owned.end()) {
            if (oit->second)
                delete d;
            else
                d->set_parent(nullptr);
            _owned.erase(oit);
        }
        
        auto it = std::find(_current_children.begin(), _current_children.end(), d);
        if (it == _current_children.end()) {
            //FormatWarning("Cannot find child of type ", d->type(), " in Entangled.");
            return;
        }
        
        *it = nullptr;
    }
    
    void Entangled::clear_children() {
        if(_begun)
            throw U_EXCEPTION("Undefined while updating.");
        
        for (size_t i=0; i<_current_children.size(); ++i) {
            if(_current_children[i]) {
                auto tmp = _current_children[i];
                tmp->set_parent(nullptr);
                
                auto it = _owned.find(tmp);
                if(it != _owned.end()) {
                    if(it->second)
                        delete tmp;
                    _owned.erase(it);
                }
                
                _current_children[i] = nullptr;
            }
        }
        
        _current_children.clear();
        _owned.clear();
        assert(_currently_removed.empty());
        
        set_content_changed(true);
    }
    
    void Entangled::init_child(Drawable* d, size_t, bool own) {
        //assert(i < _children.size());
        auto it = _currently_removed.find(d);
        if(it != _currently_removed.end())
            _currently_removed.erase(it);
        
        if(d->type() == Type::SECTION) {
            throw U_EXCEPTION("Cannot entangle Sections.");
        } else if(d->type() == Type::SINGLETON)
            throw U_EXCEPTION("Cannot entangle wrapped objects.");
        
        d->set_parent(this);
        
        _owned[d] = own;
    }
    
    void Entangled::set_bounds_changed() {
        Drawable::set_bounds_changed();
    }

}
