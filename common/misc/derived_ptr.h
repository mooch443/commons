#pragma once
#include <commons.pc.h>

namespace cmn::gui {

template<class Base>
class derived_ptr {
public:
    using element_type = Base;
    std::shared_ptr<Base> _ptr;

    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(const derived_ptr<T>& share) {
        _ptr = std::static_pointer_cast<Base>(share._ptr);
    }
    
    template<typename T>
        requires std::is_base_of_v<Base, T>
    derived_ptr(derived_ptr<T>&& share)
    {
        _ptr = std::static_pointer_cast<Base>(share._ptr);
        share.reset();
        assert(not share.has_value());
    }
    derived_ptr(const std::shared_ptr<Base>& share = nullptr) noexcept
        : _ptr(share)
    { }

   derived_ptr(std::nullptr_t) noexcept
        : _ptr(nullptr)
    {
        assert(not has_value());
    }
    
    Base& operator*() const {
        assert(get() != nullptr);
        return *get();
    }
    
    Base* get() const {
        return _ptr.get();
    }
    void reset() {
        _ptr.reset();
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
        return derived_ptr<T>(std::static_pointer_cast<T>(_ptr));
    }

    auto& get_smart() const {
        return _ptr;
    }
    
    bool has_value() const {
        return _ptr != nullptr;
    }
};


}
