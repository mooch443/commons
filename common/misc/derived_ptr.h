#pragma once
#include <commons.pc.h>

namespace cmn::gui {

template<class Base>
class derived_ptr {
public:
    using element_type = Base;
    
    std::variant<std::monostate, Base*, std::shared_ptr<Base>> _ptr{std::monostate{}};

    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(const derived_ptr<T>& share) {
        if(not share.has_value()) {
            _ptr = std::monostate{};
            assert(not has_value());
        } else if(not share.is_smart()) {
            _ptr = static_cast<Base*>(share.get());
            assert(has_value() && not is_smart());
        } else {
            _ptr = std::static_pointer_cast<Base>(share.get_smart());
            assert(has_value() && is_smart());
        }
    }
    
    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(derived_ptr<T>&& share)
    {
        if(not share.has_value()) {
            _ptr = std::monostate{};
            assert(not has_value());
        } else if(not share.is_smart()) {
            _ptr = static_cast<Base*>(share.get());
            assert(has_value() && not is_smart());
        } else {
            _ptr = std::static_pointer_cast<Base>(share.get_smart());
            assert(has_value() && is_smart());
        }
        share.reset();
        assert(not share.has_value());
    }
    derived_ptr(const std::shared_ptr<Base>& share = nullptr) noexcept
        : _ptr(share)
    {
        assert(has_value() && is_smart());
    }
    derived_ptr(Base* raw) noexcept
        : _ptr(raw)
    {
        assert(has_value() && not is_smart());
    }
    
    Base& operator*() const {
        assert(get() != nullptr);
        return *get();
    }
    
    Base* get() const {
        if(not has_value())
            return nullptr;
        else if(not is_smart()) {
            return std::get<Base*>(_ptr);
        } else
            return std::get<std::shared_ptr<Base>>(_ptr).get();
    }
    void reset() {
        _ptr = std::monostate{};
    }
    
    template<typename T>
        requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
    T* to() const;
    
    template<typename T>
        requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
    constexpr bool is() const;
    
    bool operator==(Base* raw) const { return get() == raw; }
    //bool operator==(decltype(ptr) other) const { return ptr == other; }
    bool operator==(const derived_ptr<Base>& other) const { return get() == other.get(); }
    bool operator<(Base* raw) const { return get() < raw; }
    bool operator<(const derived_ptr<Base>& other) const { return get() < other.get(); }
    
    operator bool() const { return get() != nullptr; }
    Base* operator ->() const { return get(); }
    
    template<typename T>
        requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
    operator derived_ptr<T> () {
        if(not has_value()) {
            return derived_ptr<T>();
            
        } else if(is_smart()) {
            return derived_ptr<T>(std::static_pointer_cast<T>(std::get<std::shared_ptr<Base>>(_ptr)));
        } else
            return derived_ptr<T>(static_cast<T*>(std::get<Base*>(_ptr)));
    }
    
    const std::shared_ptr<Base>& get_smart() const {
        if(not is_smart()) {
            throw U_EXCEPTION("Not holding a smart pointer right now (", Meta::name<Base>(),").");
        }
        
        return std::get<std::shared_ptr<Base>>(_ptr);
    }
    
    bool is_smart() const {
        return std::holds_alternative<std::shared_ptr<Base>>(_ptr);
    }
    bool has_value() const {
        if(_ptr.index() == std::variant_npos)
            return false;
        return not std::holds_alternative<std::monostate>(_ptr);
    }
};


}
