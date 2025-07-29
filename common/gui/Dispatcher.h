#pragma once

#include <commons.pc.h>

namespace cmn::gui {

// Attribute dispatch registry to allow late-bound (runtime) attribute application
// without having to hard-code every derived Drawable/Entangled type here.
namespace attr {
class Dispatcher {
public:
    using Key = std::pair<std::type_index, std::type_index>;
    struct PairHash {
        size_t operator()(const Key& k) const noexcept {
            return k.first.hash_code() ^ (k.second.hash_code() << 1);
        }
    };

    using Fn = std::function<bool(Drawable&, const void*)>;

    static Dispatcher& instance() {
        static Dispatcher d;
        return d;
    }

    template<class Obj, class Attr>
    void register_member() {
        std::lock_guard<std::mutex> lock(_mtx);
        _table[{std::type_index(typeid(Obj)), std::type_index(typeid(Attr))}] = [](Drawable& d, const void* a) -> bool {
            if (auto* p = dynamic_cast<Obj*>(&d)) {
                p->set(*static_cast<const Attr*>(a));
                return true;
            }
            return false;
        };
    }

    template<class Obj, class Attr, class F>
    void register_custom(F&& f) {
        std::lock_guard<std::mutex> lock(_mtx);
        _table[{std::type_index(typeid(Obj)), std::type_index(typeid(Attr))}] = [fn = std::forward<F>(f)](Drawable& d, const void* a) -> bool {
            if (auto* p = dynamic_cast<Obj*>(&d)) {
                return fn(*p, *static_cast<const Attr*>(a));
            }
            return false;
        };
    }

    template<class Attr>
    bool apply(Drawable& d, const Attr& a) const {
        // Exact dynamic-type match first
        {
            std::lock_guard<std::mutex> lock(_mtx);
            auto it = _table.find({std::type_index(typeid(d)), std::type_index(typeid(Attr))});
            if (it != _table.end())
                return it->second(d, &a);
        }
        // Fallback: try any registered object type for this attribute
        // (allows registrations on base classes or mixins)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            for (const auto& [key, fn] : _table) {
                if (key.second == std::type_index(typeid(Attr))) {
                    if (fn(d, &a)) return true;
                }
            }
        }
        return false;
    }

private:
    mutable std::mutex _mtx;
    std::unordered_map<Key, Fn, PairHash> _table;
};

// Helper macro for easy header-only registration. Uses an inline variable to avoid ODR issues.
// Note: Avoid pasting the attribute type (which might be qualified like file::PathArray)
// into an identifier. Use __COUNTER__/__LINE__ to ensure uniqueness instead.
#define CMN_GUI_PP_CAT(a,b) CMN_GUI_PP_CAT_I(a,b)
#define CMN_GUI_PP_CAT_I(a,b) a##b
#define CMN_GUI_PP_CAT3(a,b,c) CMN_GUI_PP_CAT(a, CMN_GUI_PP_CAT(b,c))

#if defined(__COUNTER__)
    #define CMN_GUI_UNIQUE_NAME2(base, obj) CMN_GUI_PP_CAT3(base##_, obj##_, __COUNTER__)
#else
    #define CMN_GUI_UNIQUE_NAME2(base, obj) CMN_GUI_PP_CAT3(base##_, obj##_, __LINE__)
#endif

#define CMN_GUI_REGISTER_ATTRIBUTE_MEMBER(OBJ, ATTR) \
    inline const bool CMN_GUI_UNIQUE_NAME2(CMN_GUI_UNIQUE_REG, OBJ) = [](){ \
        ::cmn::gui::attr::Dispatcher::instance().template register_member<OBJ, ATTR>(); \
        return true; \
    }()
} // namespace attr
    
}
