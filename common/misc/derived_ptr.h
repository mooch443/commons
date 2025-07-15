#pragma once
#include <commons.pc.h>

namespace cmn::gui {

#ifndef NDEBUG
struct DebugPointers {
    struct Zone {
        std::string name;
		size_t count_at_start = 0;
        std::unordered_set<void*> at_start;
		std::unordered_map<void*, std::string> pointers;
    };

    static std::unordered_map<std::string, std::unordered_set<void*>> allocated_pointers;
    static std::unordered_map<std::string, Zone> zones;
    static std::optional<std::string> current_zone;

    static void register_named(const std::string& key, void* ptr);
    static void unregister_named(const std::string& key, void* ptr);

    template<typename T, typename Base>
    static void register_ptr(const Base* ptr) {
        std::string key = typeid(T).name();
		register_named(key, (void*)ptr);
    }
    
    template<typename T, typename Base>
    static void unregister_ptr(const Base* ptr) {
        std::string key = typeid(T).name();
        unregister_named(key, (void*)ptr);
    }

    static void clear();
    static void start_zone(const std::string&);
	static void end_zone(const std::string&);
};
#else
struct DebugPointers {
    static inline void register_named(const std::string&, void*) {}
    static inline void unregister_named(const std::string&, void*) {}

    template<typename T, typename Base>
    static inline void register_ptr(const Base*) {}

    template<typename T, typename Base>
    static inline void unregister_ptr(const Base*) {}

    static inline void clear() {}
    static inline void start_zone(const std::string&) {}
    static inline void end_zone(const std::string&) {}
};
#endif

template<class Base>
class derived_ptr {
public:
    using element_type = Base;
    std::shared_ptr<Base> _ptr;

    template<typename T>
        requires std::is_base_of_v<Base, T> && (not std::is_same_v<T, Base>)
    derived_ptr(const derived_ptr<T>& share) {
        _ptr = std::shared_ptr<Base>(share._ptr);
    }
    
    template<typename T>
        requires std::is_base_of_v<T, Base> && (not std::is_same_v<T, Base>)
    derived_ptr(const derived_ptr<T>& share) {
		Base* ptr = dynamic_cast<Base*>(share._ptr.get());
        _ptr = std::shared_ptr<Base>(ptr, [ptr = share._ptr](auto) mutable {
            ptr = nullptr;
         });
    }
    
    derived_ptr() noexcept = default;
	derived_ptr(derived_ptr&&) noexcept = default;
	derived_ptr(const derived_ptr&) noexcept = default;

    derived_ptr& operator= (Base* ptr) noexcept {
        this->~derived_ptr();
        new (this) derived_ptr(ptr);
        return *this;
    }
	derived_ptr& operator= (derived_ptr&&) noexcept = default;
	derived_ptr& operator= (const derived_ptr&) noexcept = default;
    
    template<typename T>
        requires std::is_same_v<Base, T>
    explicit derived_ptr(std::shared_ptr<T> share) noexcept {
        if (share) {
            _ptr = std::shared_ptr<Base>(share);
        } else {
            _ptr = nullptr;
        }
    }
    
    template<typename T>
        requires std::is_same_v<Base, T>
    explicit derived_ptr(std::unique_ptr<T>&& share) noexcept {
        if (share) {
            _ptr = std::shared_ptr<Base>(
				share.release(),
				[](Base* p) {
					DebugPointers::unregister_ptr<T>(p);
					delete p;
				}
			);
        }
        else {
            _ptr = nullptr;
        }
    }

    template<typename T>
        requires std::is_base_of_v<Base, T> || std::is_same_v<Base, T>
    explicit derived_ptr(T* share) noexcept {
        if (share) {
            _ptr = std::shared_ptr<Base>(share, [share](Base* p) {
				DebugPointers::unregister_ptr<T>(p);
				delete share;
			});
            DebugPointers::register_ptr<T>(_ptr.get());
        } else {
            _ptr = nullptr;
        }
    }

   derived_ptr(std::nullptr_t) noexcept
        : _ptr(nullptr)
    {
        assert(not has_value());
    }

   ~derived_ptr() {
       _ptr = nullptr;
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
    bool operator==(const derived_ptr<Base>& other) const { return get() == other.get(); }
    bool operator<(Base* raw) const { return get() < raw; }
    bool operator<(const derived_ptr<Base>& other) const { return get() < other.get(); }
    
    operator bool() const { return get() != nullptr; }
    Base* operator ->() const { return get(); }
    
    template<typename T>
        requires (std::convertible_to<Base*, T*> || std::is_base_of_v<Base, T>)
    explicit operator derived_ptr<T> () {
        return derived_ptr<T>((const derived_ptr<Base>&)*this);
    }

    auto& get_smart() const {
        return _ptr;
    }
    
    bool has_value() const {
        return _ptr != nullptr;
    }
};

// deduction guide
template<typename T>
explicit derived_ptr(T*) -> derived_ptr<T>;

}
