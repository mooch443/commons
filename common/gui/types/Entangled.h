#pragma once

#include <commons.pc.h>
#include <gui/types/Drawable.h>
//#include <gui/GuiTypes.h>
//#include <set>
#include <misc/ranges.h>

namespace cmn::gui {
    struct Margin {
        //float left, top,
        Float2_t right, bottom;
    };
    
    //! A collection of drawables that have connected properties.
    //  If one property is updated for Entangled, it will also be updated for
    //  all objects within it. This is more efficient for a lot of simple objects
    //  inside one big container. Children / parent need to be updated less often.
    //  This removes support for Sections within Entangled, though.
    //
    //  THIS WORKS LIKE A SECTION, DIFFERENCE IS THAT WE CANNOT ENTANGLE SECTIONS
    //  OR WRAPPED OBJECTS AND CHILDREN WONT BE OFFICIALLY ADDED WITH THIS AS ITS
    //  PARENT.
    class Entangled : public SectionInterface {
    private:
        struct Context {
            Entangled* _e;
            Context(Entangled& e) : _e(&e) {
                _e->begin();
            }
            ~Context() {
                _e->end();
            }
        };
        
        friend struct Entangled::Context;
        
    public:
        [[nodiscard("Discarding the context kind of goes against the concept")]]
        inline Context OpenContext() {
            return Context(*this);
        }
        
        /// Just explicitly clearing all the children.
        inline void ClearContext() {
            Context{*this};
        }
        
        inline void OpenContext(auto&& fn) {
            Context ctx{*this};
            fn();
        }
        
    protected:
        GETTER(std::vector<Drawable*>, current_children);
        GETTER(std::vector<Drawable*>, new_children);
        robin_hood::unordered_set<Drawable*> _currently_removed;
        robin_hood::unordered_flat_set<Drawable*> _current_wrapped;
        robin_hood::unordered_map<Drawable*, bool> _owned;
        GETTER_I(std::atomic_bool, begun, false);
        
        event_handler_yes_t scrolling = [this](Event e){
            set_scroll_offset(_scroll_offset - Vec2(e.scroll.dx, e.scroll.dy));
        };
        callback_handle_t callback_ptr = nullptr;
        
        //! Scroll values in x and y direction.
        GETTER(Vec2, scroll_offset);
        //! Enables or disables scrolling
        GETTER_I(bool, scroll_enabled, false);
        
        GETTER_I(Rangef, scroll_limit_x, Rangef(0, FLT_MAX));
        GETTER_I(Rangef, scroll_limit_y, Rangef(0, FLT_MAX));
        
        //! For delta updates.
        size_t _index = 0;
        
        GETTER_I(bool, content_changed, true);
        bool _content_changed_while_updating = false;
        
    public:
        Entangled(Entangled&&) noexcept;
        Entangled& operator=(Entangled&&) noexcept;
        
        Entangled();
        Entangled(const Bounds&);
        Entangled(const std::vector<Drawable*>& objects);
        Entangled(std::function<void(Entangled&)>&&);
        void update(const std::function<void(Entangled&)> create);
        
        virtual std::string name() const { return SectionInterface::name(); }
        
        virtual ~Entangled();
        
        //! Adds object to container.
        //  Also, this takes ownership of objects.
        //Drawable* entangle(Drawable* d);
        
        bool clickable() final override {
            if(_clickable)
                return true;
            
            for(auto o : _current_children)
                if(o && o->clickable())
                    return true;
            return false;
        }
        
        //Drawable* find(float x, float y) override;
        
        void set_scroll_offset(Vec2 scroll_offset);
        void set_scroll_enabled(bool enable);
        void set_scroll_limits(const Rangef& x, const Rangef& y);
        bool empty() const;
        
        template<typename T, typename = typename std::enable_if<std::is_convertible<T, const Drawable*>::value && std::is_pointer<T>::value>::type>
        T child(size_t index) const {
            if(index >= _current_children.size())
                throw CustomException(cmn::type_v<std::invalid_argument>, "Item ",index," out of range.");
            auto ptr = dynamic_cast<T>(_current_children.at(index));
            if(!ptr)
                throw CustomException(cmn::type_v<std::invalid_argument>, "Item ", index," of type ", _current_children.at(index)->type().name(), " cannot be converted to");
            return ptr;
        }
        
        virtual void clear_children();
        virtual bool is_animating() noexcept override;
        virtual void before_draw() override;
    protected:
        friend class Section;
        
        virtual void update() {}
        
        //using SectionInterface::global_transform;
        //virtual bool global_transform(Transform &transform) override;
        
        
        
        void update_bounds() override;
        //void deinit_child(bool erase, Drawable* d);
        //void deinit_child(bool erase, std::vector<Drawable*>::iterator it, Drawable* d);
        
    private:
        //! Begin delta update
        void begin();
        //! End delta update
        void end();
        
    public:
        //! Advance one step in delta update, by adding given
        //  Drawable (and trying to match it with current one).
        //Drawable* advance(Drawable *d);
        
        const std::vector<Drawable*>& in_progress_children() const {
            return _begun ? _new_children : _current_children;
        }
        const std::vector<Drawable*>& last_children() const {
            return _current_children;
        }
        
        std::vector<Drawable*>& children() override;
        virtual void on_visibility_change(bool) override;
        
    public:
        template<typename T, class... Args, Type::data::values type = T::Class>
        T* add(Args... args) {
            static_assert(!std::is_same<Drawable, T>::value, "Dont add Drawables directly. Add them with their proper classes.");
            
            /*if(_index < _children.size()) {
                auto& current = _children[_index];
                auto owned = _owned[current];
                if(dynamic_cast<T*>(current) == NULL) {
                    
                }
            }*/
            
            T* ptr = nullptr;
            bool used_or_deleted = false;
            
            if(_index < _current_children.size()) {
                auto current = _current_children[_index];
                auto owned = _owned[current];
                _current_children[_index] = nullptr;
                
                if(owned
                   && current
                   && current->type() == type
                   /*&& dynamic_cast<T*>(current) != nullptr*/)
                { //&& current->swap_with(d)) {
                    assert(dynamic_cast<T*>(current) != nullptr);
                    static_cast<T*>(current)->create(std::forward<Args>(args)...);
                    //delete d;
                    used_or_deleted = true;
                    //d = current;
                    ptr = static_cast<T*>(current);
                    //Print("add<",type_name<T>(),">: reusing ", hex(ptr), " at ", _index);

                } else {
                    ptr = new T(std::forward<Args>(args)...);
                    if(!owned) {
                        if(current) {
                            _currently_removed.insert(current);
                            //Print("add<",type_name<T>(),">: replacing ", hex(current), " at ", _index, " with ", hex(ptr));
                        }
                        //current = ptr;
                        used_or_deleted = true;

                    } else {
                        //auto tmp = current;
                        //current = ptr;
                        //Print("add<",type_name<T>(),">: replacing ", hex(current), " at ", _index, " with ", hex(ptr));
                        
                        used_or_deleted = true;
                        current->set_parent(NULL);
                        //deinit_child(false, tmp);
                    }

                    init_child(ptr, _index, true);
                }
                
            } else {
                assert(_index == _new_children.size());
                ptr = new T(std::forward<Args>(args)...);
                used_or_deleted = true;
                //Print("add<",type_name<T>(),">: inserting new ", hex(ptr), " at ", _index);
                init_child(ptr, _index, true);
            }
            
            if(!used_or_deleted)
                throw U_EXCEPTION("Not used or deleted.");
            
            //auto ptr = insert_at_current_index(d);
            //T *ret = dynamic_cast<T*>(d);
            //assert(ret != nullptr);
            
            _set_child(ptr, true, _index);
            _index++;
            return ptr;
        }
        
        virtual void auto_size(Margin margins);
        
        void _set_child(Drawable* ptr, bool , size_t index);
        
        //! Advance in delta-update without taking ownership of objects. (Instead, copy them/match them to current object).
        template<typename T>
        void advance_wrap(T &d) {
            Drawable *ptr = &d;
            
            assert(std::find(_new_children.begin(), _new_children.end(), ptr) == _new_children.end());
            
            if(_index < _current_children.size()) {
                auto current = _current_children[_index];
                _current_children[_index] = nullptr;
                
                if(current != ptr) {
                    //Print("wrap<",type_name<T>(),">: replacing ", hex(current), " at ", _index, " with ", ptr);
                    
                    assert(!contains(_new_children, ptr));
                    if(current && _owned[current]) {
                        current->set_parent(NULL);
                        //deinit_child(false, current);
                    } else if(current) {
                        _currently_removed.insert(current);
                    }
                    
                    //current = ptr;
                    init_child(ptr, _index, false);
                    
                    _current_wrapped.insert(ptr);
                    
                    // try to see if this object already exists somewhere
                    // in the list after this
                    /*typedef decltype(_current_children.begin())::difference_type diff_t;
                    for(size_t i=_index+1; i<_current_children.size(); i++) {
                        ++iterations_dumb;
                        if(_current_children[i] == ptr) {
                            Print("\tWould clear ", hex(ptr), " from position ", i, " in _current_children");
                            //_current_children.erase(_current_children.begin() + (diff_t)i);
                            break;
                        }
                    }*/
                }
                
            } else {
                //Print("wrap<",type_name<T>(),">: inserting new ", hex(ptr), " at ", _index);
                assert(_index == _new_children.size());
                //_children.push_back(ptr);
                init_child(ptr, _index, false);
            }
            
            //assert(std::set<Drawable*>(_children.begin(), _children.end()).size() == _children.size());
            _set_child(ptr, false, _index);
            _index++;
        }
        
        void set_parent(SectionInterface* p) override;
    protected:
        
        //Drawable* insert_at_current_index(Drawable* );
        
        //! Entangled objects support delta updates by creating a new one and adding it using add_object to the DrawStructure.
        bool swap_with(Drawable* d) override;
        
        void remove_child(Drawable* d) override;
        
    public:
        virtual void set_content_changed(bool c);
        
    private:
        void init_child(Drawable* d, size_t i, bool own);
    };
}
