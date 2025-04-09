#pragma once

#include <commons.pc.h>
#include <misc/GlobalSettings.h>
#include <misc/CallbackManager.h>
#include <misc/fixed_string.h>

namespace cmn {

// Macro to generate a single setting declaration.
//#define ADD_SETTING_DECL(NAME) std::conditional<round_based, track::Settings::NAME##_t , std::optional< track::Settings::NAME##_t > > NAME;
#define ADD_SETTING_DECL(NAME) track::Settings::NAME##_t NAME;

#define ADD_SETTING_NAME(NAME) else if constexpr( ct::utils::fixed_string ( #NAME ).is_same_as( field ) ) return f( _store.NAME );

#define ADD_SETTING_NAME2(NAME) else if constexpr( Variables:: NAME == field ) return f( _store.NAME );

#define COMPARE_ENUM_NAME(NAME) else if ( field == #NAME ) return Variables:: NAME;

// Macro to generate a lambda that updates the setting.
// This lambda captures nothing and takes a pointer to the current object.
#define UPDATE_UPDATER(NAME) +[](Store& self) { \
    self.NAME = FAST_SETTING(NAME); \
}
    //self.NAME = SETTING(NAME).value<track::Settings::NAME##_t>(); \
//}

// New SIMPLE_APPLY macros to generate a comma-separated list.
#define SIMPLE_APPLY_1(m, a)         m(a)
#define SIMPLE_APPLY_2(m, a, b)      m(a), m(b)
#define SIMPLE_APPLY_3(m, a, b, c)   m(a), m(b), m(c)
#define SIMPLE_APPLY_4(m, a, b, c, d) m(a), m(b), m(c), m(d)
#define SIMPLE_APPLY_5(m, a, b, c, d, e) m(a), m(b), m(c), m(d), m(e)
#define SIMPLE_APPLY_6(m, a, b, c, d, e, f) m(a), m(b), m(c), m(d), m(e), m(f)
#define SIMPLE_APPLY_7(m, a, b, c, d, e, f, g) m(a), m(b), m(c), m(d), m(e), m(f), m(g)
#define SIMPLE_APPLY_8(m, a, b, c, d, e, f, g, h) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h)
#define SIMPLE_APPLY_9(m, a, b, c, d, e, f, g, h, i) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i)
#define SIMPLE_APPLY_10(m, a, b, c, d, e, f, g, h, i, j) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j)
#define SIMPLE_APPLY_11(m, a, b, c, d, e, f, g, h, i, j, k) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k)
#define SIMPLE_APPLY_12(m, a, b, c, d, e, f, g, h, i, j, k, l) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l)
#define SIMPLE_APPLY_13(m, a, b, c, d, e, f, g, h, i, j, k, l, m1) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1)
#define SIMPLE_APPLY_14(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n)
#define SIMPLE_APPLY_15(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o)
#define SIMPLE_APPLY_16(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o), m(p)
#define SIMPLE_APPLY_17(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o), m(p), m(q)
#define SIMPLE_APPLY_18(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o), m(p), m(q), m(r)
#define SIMPLE_APPLY_19(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o), m(p), m(q), m(r), m(s)
#define SIMPLE_APPLY_20(m, a, b, c, d, e, f, g, h, i, j, k, l, m1, n, o, p, q, r, s, t) m(a), m(b), m(c), m(d), m(e), m(f), m(g), m(h), m(i), m(j), m(k), m(l), m(m1), m(n), m(o), m(p), m(q), m(r), m(s), m(t)

#define SIMPLE_APPLY_CHOOSER(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,NAME,...) NAME
#define SIMPLE_APPLY(m, ...) SIMPLE_APPLY_CHOOSER(__VA_ARGS__, \
SIMPLE_APPLY_20, SIMPLE_APPLY_19, SIMPLE_APPLY_18, SIMPLE_APPLY_17, SIMPLE_APPLY_16, \
SIMPLE_APPLY_15, SIMPLE_APPLY_14, SIMPLE_APPLY_13, SIMPLE_APPLY_12, SIMPLE_APPLY_11, \
SIMPLE_APPLY_10, SIMPLE_APPLY_9, SIMPLE_APPLY_8, SIMPLE_APPLY_7, SIMPLE_APPLY_6, \
SIMPLE_APPLY_5, SIMPLE_APPLY_4, SIMPLE_APPLY_3, SIMPLE_APPLY_2, SIMPLE_APPLY_1)(m, __VA_ARGS__)

// Macro to generate declarations for each setting, a constexpr array of names,
// an array of updater lambdas, and an update_settings() member function.
#define ADD_SETTINGS(...)                                                       \
    static constexpr size_t N_settings = COUNT(__VA_ARGS__); \
    struct Store {  \
        MAP(ADD_SETTING_DECL, __VA_ARGS__)                                          \
    } _store; \
    \
    static inline constexpr std::array<std::string_view, N_settings>      \
        setting_names = { STRINGIZE(__VA_ARGS__) };                             \
    static inline constexpr std::array<void(*)(Store&), N_settings> \
        setting_updaters = { \
            SIMPLE_APPLY(UPDATE_UPDATER, __VA_ARGS__) \
        }; \
    enum class Variables { \
        SIMPLE_APPLY(IDENTITY, __VA_ARGS__) \
    }; \
    inline Variables get_enum_name(std::string_view field) {\
        if constexpr(false) (void)0; \
        MAP(COMPARE_ENUM_NAME, __VA_ARGS__)                                          \
        else throw RuntimeError("Cannot find field ", field, "."); \
    } \
    template< ct::utils::fixed_string field > \
    inline const auto& get() {\
        auto f = [](auto& variable) -> auto& { return variable; }; \
        if constexpr(false) (void)0; \
        MAP(ADD_SETTING_NAME, __VA_ARGS__)                                          \
        else static_assert(static_cast<int>(field) == -1, "Unknown field."); \
    } \
    template< Variables field > \
    inline const auto& get() {\
        auto f = [](auto& variable) -> auto& { return variable; }; \
        if constexpr(false) (void)0; \
        MAP(ADD_SETTING_NAME2, __VA_ARGS__)                                          \
        else static_assert(static_cast<int>(field) == -1, "Unknown field."); \
    }

template<bool round_based, bool use_atomic>
struct RBSettings {
public:
    struct ThreadObject {
        std::thread::id _thread_id{std::this_thread::get_id()};
        
        std::mutex _update_settings_mutex;
        //! everything that was changed during the previous round
        std::set<std::string_view> _updated_settings;
        
        //! saves the callback handles so we can unregister them later
        CallbackCollection _callback_collection;
        
        //! this is locked during update cycles
        bool _round_lock{false};
        
        ADD_SETTINGS(track_max_speed,
                     track_size_filter,
                     track_threshold,
                     track_speed_decay,
                     track_pause,
                     track_ignore,
                     track_include,
                     track_ignore_bdx,
                     track_only_classes,
                     track_conf_threshold,
                     track_max_individuals);
        
        std::array<std::atomic<bool>, N_settings> setting_was_updated;
        /// `Settings::<NAME>_t NAME;` ...
        
        /*ThreadObject() {
            //Print("* alloc ", hex(this));
        }*/
        ThreadObject() {
            if constexpr(use_atomic)
                std::fill(setting_was_updated.begin(), setting_was_updated.begin() + N_settings, true);
        }
        ~ThreadObject() {
            /*if(_thread_id != std::this_thread::get_id()) {
                Print("* sus ", hex(this));
            }*/
            //Print("* dealloc ", hex(this));
            std::lock_guard guard{_update_settings_mutex};
            GlobalSettings::map().unregister_callbacks(std::move(_callback_collection));
        }
        
        ThreadObject(ThreadObject&&) = delete;
        ThreadObject(const ThreadObject&) = delete;
        
        //! Call `update_settings()` to refresh any settings that changed
        void update_settings(bool force = false) {
            if(force) {
                for(auto& fn : setting_updaters) {
                    fn(_store);
                }
                return;
            }
            
            if constexpr(use_atomic) {
                for(size_t i = 0; i < N_settings; ++i) {
                    auto& value = setting_was_updated[i];
                    if(value.load()) {
                        bool expected = true;
                        if(value.compare_exchange_weak(expected, false)) {
                            setting_updaters[i](_store);
                        }
                    }
                }
                
            } else {
                std::lock_guard guard{_update_settings_mutex};
                for (auto name : _updated_settings) {
                    auto it = std::find(setting_names.begin(), setting_names.end(), name);
                    if (it != setting_names.end()) {
                        auto index = std::distance(setting_names.begin(), it);
                        setting_updaters[index](_store);
                    }
                }
            }
        }
        
        void start_round() {
#ifndef NDEBUG
            if(_round_lock)
                throw RuntimeError("Cannot start a new round before ending the previous one.");
            _round_lock = true;
#endif
            update_settings();
        }
        void end_round() {
            /// nothing
#ifndef NDEBUG
            if(not _round_lock)
                throw RuntimeError("Cannot end a round. No round was started.");
            _round_lock = false;
#endif
        }
    };
    
private:
    inline static thread_local std::shared_ptr<ThreadObject> object{[](){
        auto ptr = std::make_shared<ThreadObject>();
        //std::lock_guard guard{ptr->_update_settings_mutex};
        ptr->_callback_collection = GlobalSettings::map().
          register_callbacks<cmn::sprite::RegisterInit::DONT_TRIGGER>(
            ptr->setting_names,
            [wptr = std::weak_ptr(ptr)](auto name) {
                std::shared_ptr<ThreadObject> lock = wptr.lock();
                if(not lock)
                    return;
                
                if constexpr(use_atomic) {
                    auto var = lock->get_enum_name(name);
                    lock->setting_was_updated[static_cast<size_t>(var)] = true;
                } else {
                    std::lock_guard guard{lock->_update_settings_mutex};
                    lock->_updated_settings.insert(name);
                }
            });
        ptr->update_settings(true);
        return ptr;
    }()};
    RBSettings() {}
    
public:
    struct Guard {
        ThreadObject* settings;
        
        Guard(ThreadObject* settings)
            : settings(settings)
        {
            assert(settings->_thread_id == std::this_thread::get_id());
            settings->start_round();
        }
        
        ~Guard() {
            if(not settings)
                return; /// we have been moved from
            assert(settings->_thread_id == std::this_thread::get_id());
            settings->end_round();
        }
        
        Guard(Guard&& g) : settings(g.settings) {
            g.settings = nullptr;
        }
        Guard(const Guard&) = delete;
        Guard& operator=(Guard&& g) {
            settings = g.settings;
            g.settings = nullptr;
            return *this;
        }
        
        template<ThreadObject::Variables key>
        const auto& get() const {
            assert(settings->_thread_id == std::this_thread::get_id());
            return settings->template get<key>();
        }
        template<ct::utils::fixed_string key>
        const auto& get() const {
            assert(settings->_thread_id == std::this_thread::get_id());
            return settings->template get<key>();
        }
    };
    
    static Guard round() {
        return Guard{object.get()};
    }
};

#define RBS(NAME) round.settings->get< RB_t::ThreadObject::Variables:: NAME >()
#define RBSTR(NAME) round.settings->get< #NAME >()

}
