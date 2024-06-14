#include "Section.h"
#include <gui/DrawStructure.h>
#include <gui/DrawableCollection.h>

namespace cmn::gui {
    ColorWheel Section::wheel;

    void Section::remove_child(Drawable* d) {
        auto it = _wrapped_children.find(d);
        if(it != _wrapped_children.end()) {
            auto cit = std::find(_children.begin(), _children.end(), it->second);
            if(cit != _children.end()) {
                _children.erase(cit);
                delete it->second;
            }
            
            _wrapped_children.erase(it);
            
        } else {
            auto it = std::find(_children.begin(), _children.end(), d);
            if(it != _children.end()) {
                _children.erase(it);
                delete d;
            }
        }
    }
    
    bool Section::remove_wrapped(gui::Drawable *) {
        throw U_EXCEPTION("Unused.");
        
        /*for(auto it=_children.begin(); it != _children.end(); ++it) {
            auto c = *it;
            if(c->type() == Type::SINGLETON) {
                if(static_cast<SingletonObject*>(c)->ptr() == d) {
                    _children.erase(it);
                    delete c;
                    
                    return true;
                }
                
            } else if(c->type() == Type::SECTION) {
                if(static_cast<Section*>(c)->remove_wrapped(d))
                    return true;
            }
        }
        
        return false;*/
    }
    
    void Section::collect(std::vector<Drawable*>& ret) {
        if(not is_displayed()) {
            set_rendered(false);
            return;
        }
        
        Drawable *d;
        set_rendered(true);
        
        if(_background) {
            ret.push_back(_background);
        }
        
        for(auto c : _children) {
            if(c->type() == Type::SINGLETON)
                d = static_cast<SingletonObject*>(c)->ptr();
            else
                d = c;
            
            assert(c->parent() == this);
            
            if(d->type() == Type::ENTANGLED) {
                auto ptr = static_cast<Entangled*>(d);
                ptr->before_draw();
                
                /*if(ptr->background()) {
                    ret.push_back(ptr->background());
                }*/
                if (ptr->is_displayed()) {
                    ptr->set_rendered(true);
                    ret.push_back(ptr);
                }
                else
                    ptr->set_rendered(false);
                
            } else if(d->type() == Type::SECTION) {
                auto ptr = static_cast<Section*>(d);
                if (ptr->enabled() && ptr->is_displayed()) {
                    ptr->set_rendered(true);
                    ptr->collect(ret);
                }
                else
                    ptr->set_rendered(false);
                
            }
            else if(c->type() == Type::ENTANGLED) {
                c->set_rendered(c->is_displayed());
            }
            else if (c->is_displayed()) {
                c->set_rendered(true);
                ret.push_back(c);
            }
            else
                c->set_rendered(false);
        }
        
        if(debug_rects() && clickable() && width() > 0 && height() > 0) {
            _clr.a = hovered() ? 30 : 20;
            auto &gbounds = global_bounds();
            
            if(!prect)
                prect = new Rect(Box{gbounds}, FillClr{_clr}, LineClr{Red.alpha(125)});
            else {
                prect->set_bounds(gbounds);
                prect->set_fillclr(_clr);
            }
            
            if(!ptext)
                ptext = new Text(Str{HasName::name()}, Loc(gbounds.pos() - Vec2(0, 20)), TextClr{White}, Font(0.5));
            else {
                ptext->set_pos(gbounds.pos() - Vec2(0, 20));
            }
            
            if(!stext) {
                stext = new Text();
                stext->set_font(Font(0.5, Align::Center));
            }
            stext->set_txt(std::to_string(int(location().x))+","+std::to_string(int(location().y))+" "+std::to_string(int(width()))+"x"+std::to_string(int(height())));
            stext->set_pos(gbounds.pos()+gbounds.size()*0.5f);
            
            //prect->update_bounds();
            //ptext->update_bounds();
            //stext->update_bounds();
            
            ret.push_back(prect);
            ret.push_back(ptext);
            ret.push_back(stext);
        }
    }
    
    Section* Section::find_section(const std::string& name) const {
        if(this->HasName::name() == name)
            return const_cast<Section*>(this);
        
        for(auto c : _children) {
            if(c->type() == Type::SINGLETON)
                c = static_cast<SingletonObject*>(c)->ptr();
            
            if(c->type() == Type::SECTION) {
                auto f = static_cast<Section*>(c)->find_section(name);
                if(f)
                    return f;
            }
        }
        
        return NULL;
    }
    
    void Section::children_rect_changed() {
        SectionInterface::children_rect_changed();
    }
    
    void Section::begin(bool reuse) {
        _enabled = true;
        if(!_was_enabled)
            children_rect_changed();
        
        if(reuse) {
            return;
        }
        
        _index = 0;
        
        // disable all sections until they are "begun"
        for(size_t i=0; i<_children.size(); i++) {
            const auto &c = _children[i];
            if(c->type() == Type::SECTION) {
                static_cast<Section*>(c)->_was_enabled = static_cast<Section*>(c)->enabled();
                static_cast<Section*>(c)->set_enabled(false);
            }
        }
    }
    
    void Section::add_collection(DrawableCollection *custom, bool wrap) {
        Drawable *d = custom;
        
        // special case for custom drawable collections
        if(!wrap) {
            // the object is simply the collection
            if(_children.size() > _index
               && _children.at(_index) == custom)
            {
                // do nothing
            } else {
                // serach for the object
                for(auto it = _children.begin(); it != _children.end(); ++it) {
                    if(*it == custom) {
                        _children.erase(it);
                        if(size_t(it - _children.begin()) < _index)
                            _index--;
                        break;
                    }
                }
                
                _children.insert(_children.begin() + _index, custom);
            }
            
        } else {
            // the object is a singleton (section is to be wrapped)
            auto current = _children.size() > _index ? _children.at(_index) : NULL;
            if(current != nullptr
               && current->type() == Type::SINGLETON
               && static_cast<SingletonObject*>(current)->ptr() == custom)
            {
                // do nothing
                d = current;
                
            } else {
                // search for the object
                for(auto it = _children.begin(); it != _children.end(); ++it) {
                    if((*it)->type() == Type::SINGLETON
                       && static_cast<SingletonObject*>(*it)->ptr() == custom)
                    {
                        d = *it;

						auto start = _children.begin();
						if (abs(std::distance(it, start)) < (long)_index && _index > 0)
							//if(size_t(it - _children.begin()) < _index)
							--_index;
                        else if(size_t(it - _children.begin()) < _index)
                            throw U_EXCEPTION("problem: ",std::distance(it, start)," vs ", size_t(it - _children.begin()));

                        _children.erase(it);
                        
                        break;
                    }
                }
                
                if(d == custom)
                    d = new SingletonObject(custom);
                _wrapped_children[custom] = static_cast<SingletonObject*>(d);
                
                _children.insert(_children.begin() + _index, d);
            }
        }
        
        if(d->parent() != this)
            d->set_parent(this);
        
        _index++;
        
        // instantly add content of section using update()
        stage()->push_section(custom);
        custom->_was_enabled = true;
        custom->update(*stage());
        stage()->pop_section();
        // drawing done
    }
    
    void Section::wrap_object(Drawable* d) {
        // check if the current object is a collection
        // if yes, special case for adding
        if(d->type() == Type::SECTION) {
            assert(dynamic_cast<DrawableCollection*>(d));
            add_collection(static_cast<DrawableCollection*>(d), true);
            return;
        }
        
        SingletonObject *obj = find_wrapped<SingletonObject>(d);
        if(!obj)
            return;
        
        assert(!contains(_children, obj));
        assert(_index <= _children.size());
        _children.insert(_children.begin() + _index, obj);
        _wrapped_children[d] = obj;
        _index++;
    }
    
    void Section::end() {
        // root has to draw dialogs as well
        if(!parent() && stage())
            stage()->update_dialogs();
        
        // delete all excess objects
        auto index = _index;
        while(_children.size() > index) {
            if(_children.at(index)->type() == Type::SECTION) {
                ++index;
                
            } else {
                auto ptr = _children.at(index);
                if(ptr->type() == Type::SINGLETON) {
                //    _wrapped_children.erase(static_cast<SingletonObject*>(ptr)->ptr());
                    ptr = static_cast<SingletonObject*>(ptr)->ptr();
                }
                
                if(ptr->parent()) {
                    ptr->set_parent(nullptr);
                    //assert(_children.at(index) == ptr);
                    //_children.erase(_children.begin() + index);
                    
                } else
                    ++index;
            }
        }
        
        /*for(auto it = _children.begin() + _index; it != _children.end(); ) {
            auto obj = *it;
            if(obj->type() != Type::SECTION) {
                it = _children.erase(it);
                if(obj->type() == Type::SINGLETON) {
                    _wrapped_children.erase(static_cast<SingletonObject*>(obj)->ptr());
                }
                
                obj->clear_parent_dont_check();
                delete obj;
                
            } else
                ++it;
        }*/
        
        if(_index > _children.size())
            _index = _children.size();
    }
    
    void Section::reuse_current_object() {
        if(_index < _children.size()) {
            if(_children.at(_index)->type() == Type::SECTION) {
                auto section = static_cast<Section*>(_children.at(_index));
                
                if(section->was_enabled()) {
                    section->set_enabled(true);
                    while(section->_index < section->children().size())
                        section->reuse_current_object();
                }
            }
            
            _index++;
        }
    }
    
    void Section::update_bounds() {
        if(!_bounds_changed)
            return;
        
        _section_clickable = false;
        
        for(auto ptr : _children) {
            // use actual object instead
            if(ptr->type() == Type::SINGLETON)
                ptr = static_cast<SingletonObject*>(ptr)->ptr();
            
            if(ptr->clickable()) {
                _section_clickable = true;
                break;
            }
        }
        
        SectionInterface::update_bounds();
    }
    
    void Section::find(float x, float y, std::vector<Drawable*>& results) {
        if(!enabled())
            return;
        
        SectionInterface::find(x, y, results);
    }
    
    void Section::clear() {
        //DrawStructure::Lock_t *guard = NULL;
        //if(stage())
        //    guard = new GUI_LOCK(stage()->lock());
        
        // Copy first to prevent changing the list while clearing it
        // through any of the deleted children.
        auto copy = _children;
        _children.clear();
        _wrapped_children.clear();
        
        for(auto c: copy)
            delete c;
        
        //if(guard)
        //    delete guard;
    }
    
    Section::~Section() {
        //DrawStructure::Lock_t *guard = NULL;
        //if(stage())
        //    guard = new GUI_LOCK(stage()->lock());
        
        if(stage() && stage()->active_section() == this)
            stage()->pop_section();
        
        auto children = _children;
        _children.clear();
        _wrapped_children.clear();
        
        for(auto c: children) {
            if(c->type() == Type::SECTION) {
                //! TODO: Untested whether the change from static_cast to dynamic_cast does anything, but cannot see what that would be.
                auto ptr = dynamic_cast<DrawableCollection*>(c);
                if(ptr) {
                    // dont delete DrawableCollections...
                    //ptr->set_parent(NULL);
                    ptr->clear_parent_dont_check();
                    continue;
                }
            }
            
            delete c;
        }
        
        //if(guard)
        //    delete guard;
        
        if(prect)
            delete prect;
        if(ptext)
            delete ptext;
    }
    
    void Section::structure_changed(bool downwards) {
        if(downwards) {
            for(auto c : _children)
                c->structure_changed(true);
        }
        
        SectionInterface::structure_changed(downwards);
    }
}
