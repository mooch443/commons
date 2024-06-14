#pragma once
#include <commons.pc.h>

namespace cmn::gui {

template<class Base>
class derived_ptr {
public:
    std::shared_ptr<Base> ptr;
    Base* raw_ptr;

    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(const derived_ptr<T>& share)
        : ptr(share.ptr), raw_ptr(share.raw_ptr)
    {}
    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(derived_ptr<T>&& share)
        : ptr(std::move(share.ptr)), raw_ptr(std::move(share.raw_ptr))
    {}
    derived_ptr(const std::shared_ptr<Base>& share = nullptr) : ptr(share), raw_ptr(nullptr) {}
    derived_ptr(Base* raw) : ptr(nullptr), raw_ptr(raw) {}
    
    Base& operator*() const { return ptr ? *ptr : *raw_ptr; }
    Base* get() const { return ptr ? ptr.get() : raw_ptr; }
    template<typename T> T* to() const {
        auto ptr = dynamic_cast<T*>(get());
        if(!ptr)
            throw std::runtime_error("Cannot cast object to specified type.");
        return ptr;
    }
    
    template<typename T>
    constexpr bool is() const { return dynamic_cast<T*>(get()); }
    
    bool operator==(Base* raw) const { return get() == raw; }
    //bool operator==(decltype(ptr) other) const { return ptr == other; }
    bool operator==(derived_ptr<Base> other) const { return get() == other.get(); }
    bool operator<(Base* raw) const { return get() < raw; }
    bool operator<(derived_ptr<Base> other) const { return get() < other.get(); }
    
    operator bool() const { return get() != nullptr; }
    Base* operator ->() const { return get(); }
    
    template<typename T>
    operator derived_ptr<T> () {
        if(ptr)
            return derived_ptr<T>(std::static_pointer_cast<T>(ptr));
        return derived_ptr<T>(static_cast<T*>(raw_ptr));
    }
};


}
