#include "SingletonObject.h"
#include <gui/DrawStructure.h>
#include <gui/Section.h>

namespace cmn::gui {
    SingletonObject::SingletonObject(Drawable *d)
        : Drawable(Type::SINGLETON),
         _ptr(d)
    {
        assert(d);
        d->add_custom_data("singleton", (void*)this);
    }

    SingletonObject::~SingletonObject() {
        if(_ptr) {
            _ptr->remove_custom_data("singleton");
            _ptr->set_parent(NULL);
        }
    }
    
    void SingletonObject::set_parent(gui::SectionInterface *p) {
        Drawable::set_parent(p);
        if(_ptr)
            _ptr->set_parent(p);
    }
    
    std::ostream& SingletonObject::operator<< (std::ostream &os)
    {
        if(not _ptr)
            return os;
        return _ptr->operator<<(os);
    }
    
    void SingletonObject::set_bounds_changed() {
        if(_ptr)
            _ptr->set_bounds_changed(); // send down to actual object
    }
    
    void SingletonObject::update_bounds() {
        if(_ptr)
            _ptr->update_bounds();
    }
    
    void SingletonObject::clear_parent_dont_check() {
        Drawable::clear_parent_dont_check();
        if(_ptr)
            _ptr->clear_parent_dont_check();
    }

bool SingletonObject::is_animating() noexcept {
    return _ptr && _ptr->is_animating();
}
}
