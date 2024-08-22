#pragma once

#include <commons.pc.h>
#include <gui/types/Layout.h>
#include <gui/types/Drawable.h>

namespace cmn::gui {

class Fallthrough : public Drawable {
    GETTER_NCONST(Layout::Ptr, object);
    
public:
    Fallthrough() : Drawable(Type::PASSTHROUGH) { }
    
    //using Entangled::set;
    //template<typename T>
    //void set(T&& t);
    void set_name(const std::string& n) override {
        if(_object) {
            _object->set_name(n);
        }
        Drawable::set_name("FT<"+n+">)");
    }
    
    void set_bounds(const Bounds& bds) override {
        if(_object)
            _object->set_bounds(bds);
    }
    void set_size(const Size2& size) override {
        if(_object)
            _object->set_size(size);
    }
    void set_pos(const Vec2& size) override {
        if(_object)
            _object->set_pos(size);
    }
    
    void children_rect_changed() {
        if(not _object)
            return;
        if(_object.is<SectionInterface>())
            _object.to<SectionInterface>()->set_bounds_changed();
        else if(_object.is<Fallthrough>())
            _object.to<Fallthrough>()->children_rect_changed();
    }
    
    void set_bounds_changed() override {
        if(_object)
            _object->set_bounds_changed();
        Drawable::set_bounds_changed();
    }
    
    void set_object(const Layout::Ptr& o) {
        if(o == _object)
            return;
        
        if(_object) {
            _object->remove_custom_data("passthrough");
            _object->set_parent(nullptr);
        }
        
        if(o.is<Fallthrough>()) {
            //throw U_EXCEPTION("Cannot add PASSTHROUGH to a PASSTHROUGH");
        }
        _object = o;
        
        if(_object) {
            _object->add_custom_data("passthrough", (void*)this);
            _object->set_parent(parent());
        }
    }
    
    void update_bounds() override {
        if(not _bounds_changed)
            return;
        
        Drawable::set_bounds(_object ? _object->bounds() : Bounds{});
        Drawable::update_bounds();
    }
    
    void set_parent(SectionInterface* parent) override {
        Drawable::set_parent(parent);
        if(_object)
            _object->set_parent(parent);
    }
};

/*template<typename T>
void Fallthrough::set(T&& t) {
    if(_object)
        LabeledField::delegate_to_proper_type(t, _object);
        //_object->set(std::forward<T>(t));
}*/

template<typename Base>
template<typename T>
    requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
T* derived_ptr<Base>::to() const {
    Base* c = get();
    if constexpr(not std::same_as<T, Fallthrough> && std::same_as<Base, Drawable>) {
        while(c && static_cast<Base*>(c)->type() == Type::PASSTHROUGH) {
            c = ((Fallthrough*)c)->object().get();
        }
    }
    
    auto ptr = dynamic_cast<T*>(c);
    if(!ptr)
        throw std::runtime_error("Cannot cast object to specified type.");
    return ptr;
}

template<typename Base>
template<typename T>
    requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
constexpr bool derived_ptr<Base>::is() const {
    auto c = get();
    if constexpr(not std::same_as<T, Fallthrough> && std::same_as<Base, Drawable>) {
        while(c && c->type() == Type::PASSTHROUGH) {
            c = static_cast<Fallthrough*>(c)->object().get();
        }
    }
    return dynamic_cast<T*>(c) != nullptr;
}

template<typename T, typename = void>
struct retrieve_ptr_type {
    using type = T;
};

template<typename T>
struct retrieve_ptr_type<T, std::enable_if_t<not _is_dumb_pointer<T>>> {
    using type = typename T::element_type*;
};

template<
    typename F,
    typename R = std::invoke_result_t<F, Drawable*>
>
R apply_to_object(Drawable* c, F&& fn) {
    if(not c) {
        if constexpr(std::same_as<R, void>)
            return;
        else
            return R{};
    }

    if(c->type() == Type::SINGLETON) {
        c = static_cast<SingletonObject*>(c)->ptr();
    }
    while(c->type() == Type::PASSTHROUGH) {
        c = static_cast<Fallthrough*>(c)->object().get();
        if(not c) {
            if constexpr(std::same_as<R, void>)
                return;
            else
                return R{};
        }
    }
    
    assert(dynamic_cast<Fallthrough*>(c) == nullptr);

    if constexpr(std::same_as<R, void>)
        fn(c);
    else
        return fn(c);
}

template<
    typename T,
    typename... Args,
    template<typename, typename...> class Array,
    typename F,
    typename R = std::invoke_result_t<F, Drawable*>
>
R apply_to_objects(const Array<T, Args...>& array, F&& fn) {
    if constexpr(std::same_as<R, void>) {
        if constexpr(_is_dumb_pointer<T>) {
            for(T c : array) {
                apply_to_object(c, fn);
            }
        } else {
            for(const T& c : array) {
                if(not c)
                    continue;
                
                apply_to_object(c.get(), fn);
            }
        }
        
    } else {
        if constexpr(_is_dumb_pointer<T>) {
            for(T c : array) {
                R r = apply_to_object(c, fn);
                if(r)
                    return r;
            }
        } else {
            for(const T& c : array) {
                if(not c)
                    continue;
                
                R r = apply_to_object(c.get(), fn);
                if(r)
                    return r;
            }
        }
        
        return R{};
    }
}

}
