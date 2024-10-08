#pragma once

#include <gui/types/Drawable.h>

namespace cmn::gui {
    class HasName {
        GETTER_SETTER(std::string, name);
    public:
        HasName(const std::string& name) : _name(name) {}
        virtual ~HasName() {}
    };
    
    class Section;
    
    class SingletonObject : public Drawable {
    protected:
        GETTER_PTR(Drawable*, ptr);
        
    public:
        SingletonObject(Drawable *d);
        ~SingletonObject();
        
        bool swap_with(Drawable* draw) override {
            auto d = dynamic_cast<SingletonObject*>(draw);
            if(d) {
                assert(d->ptr() == _ptr);
                // No swapping needs to be done here
                
            } else {
                throw U_EXCEPTION("Can only be swapped with Singletons.");
            }
            
            return true;
        }
        
        virtual void set_bounds_changed() override;
        virtual void update_bounds() override;
        virtual void set_dirty() final override {
            if(_ptr)
                _ptr->set_dirty();
        }
        virtual bool is_animating() noexcept override;
        
        virtual void set_parent(SectionInterface*) final override;
        std::ostream& operator<< (std::ostream &os) override;
        
    protected:
        void clear_parent_dont_check() override;
    };
}
