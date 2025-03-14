#include "Entangled.h"
#include <gui/DrawStructure.h>
#include <gui/types/Dropdown.h>
#include <misc/stacktrace.h>
#include <misc/Timer.h>
#include <gui/Passthrough.h>

namespace cmn::gui {
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
        OpenContextWithArgs(create, *this);
    }
    
    Entangled::~Entangled() {
        auto children = this->children();
        _current_children.clear();
        _new_children.clear();
        
        if(stage() && stage()->selected_object()
           && (stage()->selected_object() == this || stage()->selected_object()->is_child_of(this)))
        {
            stage()->selected_object() = nullptr;
        }
        
        if(stage() && stage()->hovered_object()
           && (stage()->hovered_object() == this || stage()->hovered_object()->is_child_of(this)))
        {
            stage()->hovered_object() = nullptr;
        }
        
        set_stage(nullptr);
        
        apply_to_objects(children, [](auto c){
            c->set_parent(nullptr);
        });
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
    
    //assert(not contains(_current_children, ptr));
}

void Entangled::on_visibility_change(bool visible) {
    SectionInterface::set_rendered(visible);
    for(auto ptr : children()) {
        if(ptr)
            ptr->set_rendered(visible);
    }
}

    void Entangled::begin() {
        bool expected = false;
        if(not _begun.compare_exchange_strong(expected, true)) {
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
            _current_children[i] = nullptr;
            
            if(tmp) {
                //iterations_smart +=_current_wrapped.empty() ? 0 : log(_current_wrapped.size());
                if(auto it = _current_wrapped.find(tmp);
                   it != _current_wrapped.end())
                {
                    //Print("Ignoring past-end of _current_children: ", hex(tmp));
                    assert(std::find(_new_children.begin(), _new_children.begin() + _index, tmp) != _new_children.begin() + _index);
                    _current_wrapped.erase(it);
                    
                    if(auto it = _currently_removed.find(tmp);
                       it != _currently_removed.end()) 
                    {
                        _currently_removed.erase(it);
                    }
                    continue;
                }
                //Print("Past end of _current_children: ", hex(tmp));
                
#ifndef NDEBUG
                for(size_t i=0; i<_index; ++i) {
                    assert(_new_children.at(i) != tmp);
                }
#endif
                
                bool owned = false;
                auto it = _owned.find(tmp);
                if(it != _owned.end()) {
                    if(it->second)
                        owned = true;
                    _owned.erase(it);
                }
                
                tmp->set_parent(NULL);
                
                if(owned)
                    delete tmp;
            }
        }
        
        while(!_currently_removed.empty()) {
            auto d = *_currently_removed.begin();
            _currently_removed.erase(_currently_removed.begin());
            
            if (!d)
                continue;
            
            //iterations_smart += log(_current_wrapped.size());
            if(auto it = _current_wrapped.find(d);
               it != _current_wrapped.end())
            {
                _current_wrapped.erase(it);
                //Print("Ignoring currently_removed: ", hex(d));
                continue;
            }
            
#ifndef NDEBUG
            //Print("Removing ", hex(d));
            for(size_t i=0; i<_index; ++i) {
                assert(_new_children.at(i) != d);
            }
#endif
            
            bool owned = false;
            auto it = _owned.find(d);
            if(it != _owned.end()) {
                if(it->second)
                    owned = true;
                _owned.erase(it);
            }
            
            d->set_parent(NULL);
            
            if(owned)
                delete d;
        }
        
        _new_children.resize(_index);
        std::swap(_current_children, _new_children);
        _new_children.clear();
        _current_wrapped.clear();
        
        //if(iterations_dumb > 0 || iterations_smart > 0)
        //    Print("Iterations: ", iterations_dumb, " vs. ", iterations_smart, " (", int64_t(iterations_dumb) - int64_t(iterations_smart),")");
        
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
            
            if(dynamic_cast<const Tooltip*>(c))
                continue;
            
            auto bds = c->local_bounds();
            mi = min(bds.pos(), mi);
            ma = max(bds.pos() + bds.size(), ma);
        }
        
        ma += Vec2(max(0.f, margin.right), max(0.f, margin.bottom));
        set_size(ma);
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

bool Entangled::is_animating() noexcept {
    if(Drawable::is_animating())
        return true;
    return apply_to_objects(children(), [](Drawable* c){
        return c->is_animating();
    });
}

void Entangled::before_draw() {
#ifndef NDEBUG
    static size_t iterations{0};
    static Timer last_print;
        
    ++iterations;
#endif
    _content_changed_while_updating = false;
    update();

    apply_to_objects(children(), [](Drawable* c){
        if(c->type() == Type::SECTION || c->type() == Type::ENTANGLED) {
            static_cast<SectionInterface*>(c)->before_draw();
        }
    });

    SectionInterface::before_draw();
        
    if(_content_changed && !_content_changed_while_updating)
        _content_changed = false;
    else if(_content_changed_while_updating)
        set_dirty();
    
#ifndef NDEBUG
    if(last_print.elapsed() > 10) {
        Print("Iterations[Entangled::before_draw] = ", iterations);
        last_print.reset();
    }
#endif
}
    
void SectionInterface::before_draw() {
#ifndef NDEBUG
    static size_t iterations{0}, processed_objects{0}, Nsamples{0};
    static Timer last_print;
#endif
    
    if(_has_children_rect_changed) {
        std::vector<SectionInterface*> reset;
        std::queue<SectionInterface*> q;
        q.push(this);
        reset.push_back(this);
        
        while(not q.empty()) {
            auto ptr = q.front();
            q.pop();
            
#ifndef NDEBUG
            ++iterations;
#endif
            ptr->_has_children_rect_changed = true;
            reset.push_back(ptr);
            ptr->set_bounds_changed();
            //ptr->children_rect_changed();
            
            if(ptr->_background)
                ptr->_background->set_bounds_changed();
            
            apply_to_objects(ptr->children(), [&q](auto c){
                if(c->type() == Type::ENTANGLED || c->type() == Type::SECTION) {
                    q.push(static_cast<SectionInterface*>(c));
                } else
                    c->set_bounds_changed();
            });
        }
        
        /// we need to reset the has *children_rect_changed*
        /// after the loop because otherwise it'll *set_bounds_dirty*
        /// from some child, which resets the parent (that we just processed)
        for(auto r : reset) {
            r->_has_children_rect_changed = false;
        }
        
#ifndef NDEBUG
        processed_objects += reset.size();
        ++Nsamples;
        if(last_print.elapsed() > 10) {
            Print("Iterations[before_draw] = ", iterations, " with ", processed_objects / double(Nsamples), " objects/run");
            last_print.reset();
        }
#endif
    }
    
    _has_children_rect_changed = false;
}

void Entangled::set_parent(SectionInterface* p) {
    //children_rect_changed();
    if(p != _parent) {
        if(p)
            set_content_changed(true);
        
        if(not p)
            set_stage(nullptr);
        SectionInterface::set_parent(p);
    }
}

void SectionInterface::children_rect_changed() {
    if(_has_children_rect_changed)
        return;
    
#ifndef NDEBUG
    static size_t iterations{0};
    static Timer last_print;
#endif
    
    _has_children_rect_changed = true;
    
    set_bounds_changed();
    if(_background)
        _background->set_bounds_changed();
    
#ifndef NDEBUG
    if(last_print.elapsed() > 10) {
        Print("Iterations[children_rect_changed] = ", iterations);
        last_print.reset();
    }
#endif
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
        
        if(stage()) {
            if(stage()->selected_object() == d
               || (stage()->selected_object()
                   && stage()->selected_object()->is_child_of(d)))
            {
                stage()->selected_object() = nullptr;
            }
            if(stage()->hovered_object()  == d
               || (stage()->hovered_object()
                   && stage()->hovered_object()->is_child_of(d)))
            {
                stage()->hovered_object() = nullptr;
            }
        }
        
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
        
        auto cchildren = _current_children;
        _current_children.clear();
        
        for (size_t i=0; i<cchildren.size(); ++i) {
            apply_to_object(cchildren[i], [this](Drawable* tmp) {
                tmp->set_parent(nullptr);
                
                auto it = _owned.find(tmp);
                if(it != _owned.end()) {
                    if(it->second)
                        delete tmp;
                    _owned.erase(it);
                }
            });
        }
        
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
        /*if(d->type() == Type::PASSTHROUGH) {
            auto c = static_cast<Fallthrough*>(d)->object().get();
            if(c)
                c->set_parent(this);
        }else*/
            d->set_parent(this);
        
        _owned[d] = own;
    }

bool Entangled::clickable() {
    if(_clickable)
        return true;
    
    return apply_to_objects(_current_children, [](auto o) -> bool {
        if(o->clickable())
            return true;
        return false;
    });
}

}
