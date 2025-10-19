/**
 * @file RBSettings.h
 * @brief Round-based and atomic-capable settings management with thread-local isolation and efficient update tracking.
 *
 * Provides a generic way to define, access, and update named configuration settings with support for
 * round-based updates or atomic async update notification using a lock-free MPSC queue.
 */
#pragma once

#include <commons.pc.h>
#include <misc/GlobalSettings.h>
#include <misc/CallbackManager.h>
#include <misc/fixed_string.h>

namespace cmn {

/**
 * @brief Lock-free multi-producer, single-consumer queue.
 *
 * Used to track updated settings when operating in atomic update mode.
 * Safe for multiple producers (callback threads) and one consumer (the updater).
 *
 * @tparam T Type of the elements in the queue.
 * @tparam Capacity Maximum number of elements before overwriting occurs.
 */
template <typename T, size_t Capacity>
class MPSCQueue {
public:
    MPSCQueue() : head(0), tail(0) {}

    // Called from the callback (multiple producers)
    bool push(const T& item) {
        // Load the current tail and head with proper memory order
        size_t current_tail = tail.load(std::memory_order_relaxed);
        size_t current_head = head.load(std::memory_order_acquire);
        // Check if there is space; note: you must choose a Capacity that never gets exceeded
        if (current_tail - current_head >= Capacity) {
            return false; // Queue full
        }
        // Reserve the slot by fetching the tail index atomically.
        size_t pos = tail.fetch_add(1, std::memory_order_relaxed);
        buffer[pos % Capacity] = item;
        // The store to buffer is visible once the tail is updated.
        return true;
    }

    // Called from the consumer (single thread in round() updater)
    bool pop(T& out) {
#ifndef NDEBUG
        if (!consumer_initialized) {
            consumer_thread = std::this_thread::get_id();
            consumer_initialized = true;
        } else {
            assert(std::this_thread::get_id() == consumer_thread &&
                   "MPSCQueue pop() must be called from a single consumer thread");
        }
#endif
        size_t current_head = head.load(std::memory_order_relaxed);
        if (current_head >= tail.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        out = buffer[current_head % Capacity];
        head.store(current_head + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> buffer;
    std::atomic<size_t> head; // consumer's read position
    std::atomic<size_t> tail; // producers' write position
    
#ifndef NDEBUG
    std::thread::id consumer_thread;
    bool consumer_initialized = false;
#endif
};

#include <misc/PBSettingsMacros.h>

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
    //self.NAME = READ_SETTING(NAME, track::Settings::NAME##_t); \
//}

#undef STRINGIZE_SINGLE
#define STRINGIZE_SINGLE(NAME) IDENTITY(#NAME)

/**
 * @brief Macro to generate settings structure and associated metadata.
 *
 * Generates:
 * - The actual storage container (`Store`)
 * - Enum of setting names
 * - Arrays of setting names and update lambdas
 * - Accessors to retrieve a setting by name or enum
 *
 * @param __VA_ARGS__ Names of the settings to register.
 */
#define ADD_SETTINGS(...)                                                       \
    static constexpr size_t N_settings = COUNT(__VA_ARGS__); \
    struct Store {  \
        _SIMPLE_APPLY(ADD_SETTING_DECL, _SEP_EMPTY, __VA_ARGS__)                                          \
    } _store; \
    \
    static inline constexpr std::array<std::string_view, N_settings>      \
        setting_names = { \
        SIMPLE_APPLY(STRINGIZE_SINGLE, __VA_ARGS__) \
    };  \
    static inline constexpr std::array<void(*)(Store&), N_settings> \
        setting_updaters = { \
            _SIMPLE_APPLY(UPDATE_UPDATER, _SEP_INDIRECT, __VA_ARGS__) \
        }; \
    enum class Variables { \
        SIMPLE_APPLY(IDENTITY, __VA_ARGS__) \
    }; \
    inline Variables get_enum_name(std::string_view field) {\
        if constexpr(false) (void)0; \
        _SIMPLE_APPLY(COMPARE_ENUM_NAME, _SEP_EMPTY, __VA_ARGS__)                                          \
        else throw RuntimeError("Cannot find field ", field, "."); \
    } \
    template< ct::utils::fixed_string field > \
    inline const auto& get() {\
        auto f = [](auto& variable) -> auto& { return variable; }; \
        if constexpr(false) (void)0; \
        _SIMPLE_APPLY(ADD_SETTING_NAME, _SEP_EMPTY, __VA_ARGS__)                                          \
        else static_assert(static_cast<int>(field) == -1, "Unknown field."); \
    } \
    template< Variables field > \
    inline const auto& get() {\
        auto f = [](auto& variable) -> auto& { return variable; }; \
        if constexpr(false) (void)0; \
        _SIMPLE_APPLY(ADD_SETTING_NAME2, _SEP_EMPTY, __VA_ARGS__)                                          \
        else static_assert(static_cast<int>(field) == -1, "Unknown field."); \
    }

// Define a helper that can be specialized based on whether to use atomic.
template<bool UseAtomic>
struct RBUpdateStateHelper;  // Primary template (unspecialized)

// Specialization for the atomic variant (true): no members.
template<>
struct RBUpdateStateHelper<true> {
    // Empty: atomic variant has no members.
};

// Specialization for the non-atomic variant (false):
template<>
struct RBUpdateStateHelper<false> {
    std::mutex _update_settings_mutex;
    std::set<std::string_view> _updated_settings;
};

 /**
  * @brief Thread-local round-based or atomic-capable settings manager.
  *
  * @tparam round_based If true, settings are only updated at round boundaries.
  * @tparam use_atomic If true, updated settings are tracked via MPSC queue; else with mutex-protected set.
  */
template<bool round_based, bool use_atomic>
struct RBSettings {
    using UpdateState = RBUpdateStateHelper<use_atomic>;
    
public:
    /**
     * @brief Thread-local object that owns actual settings and handles updates.
     *
     * Includes setting storage, callback registration, update tracking, and round lifecycle support.
     */
    struct ThreadObject : public UpdateState {
        std::atomic<size_t> external_access_count{0};
        std::thread::id _thread_id{std::this_thread::get_id()};
        
        //! saves the callback handles so we can unregister them later
        cmn::CallbackFuture _callback_collection;
        
#ifndef NDEBUG
        //! this is locked during update cycles
        bool _round_lock{false};
#endif
        
        ADD_SETTINGS(smooth_window,
                     cm_per_pixel,
                     frame_rate,
                     track_enforce_frame_rate,
                     track_max_reassign_time,
                     speed_extrapolation,
                     calculate_posture,
                     track_max_speed,
                     track_size_filter,
                     track_threshold,
                     track_threshold_2,
                     threshold_ratio_range,
                     track_max_individuals,
                     track_posture_threshold,
                     outline_smooth_step,
                     outline_smooth_samples,
                     outline_resample,
                     manual_matches,
                     manual_splits,
                     track_ignore_bdx,
                     midline_resolution,
                     meta_mass_mg,
                     individual_names,
                     midline_stiff_percentage,
                     match_min_probability,
                     match_topk,
                     posture_direction_smoothing,
                     tags_path,
                     grid_points,
                     recognition_shapes,
                     grid_points_scaling,
                     track_ignore,
                     track_include,
                     tracklet_punish_timedelta,
                     huge_timestamp_seconds,
                     manually_approved,
                     track_speed_decay,
                     midline_invert,
                     track_time_probability_enabled,
                     posture_head_percentage,
                     track_threshold_is_absolute,
                     track_background_subtraction,
                     blobs_per_thread,
                     individual_prefix,
                     video_length,
                     analysis_range,
                     visual_field_eye_offset,
                     visual_field_eye_separation,
                     visual_field_history_smoothing,
                     tracklet_punish_speeding,
                     match_mode,
                     track_do_history_split,
                     posture_closing_steps,
                     posture_closing_size,
                     individual_image_scale,
                     track_pause,
                     track_trusted_probability,
                     accumulation_tracklet_add_factor,
                     output_interpolate_positions,
                     track_consistent_categories,
                     categories_ordered,
                     track_only_categories,
                     track_only_classes,
                     track_conf_threshold,
                     tracklet_max_length,
                     individual_image_size,
                     categories_train_min_tracklet_length,
                     categories_apply_min_tracklet_length,
                     meta_encoding,
                     outline_compression,
                     image_invert);
        
        //std::array<std::atomic<bool>, N_settings> setting_was_updated;
        /// `Settings::<NAME>_t NAME;` ...
        
        // Choose a capacity that fits your expected burst of updates.
        static constexpr size_t QueueCapacity = 128;
        MPSCQueue<Variables, QueueCapacity> updatedQueue;
        
        ThreadObject() = default;
        ~ThreadObject() {
            GlobalSettings::unregister_callbacks(std::move(_callback_collection));
        }
        
        ThreadObject(ThreadObject&&) = delete;
        ThreadObject(const ThreadObject&) = delete;
        
        /**
         * @brief Applies pending setting updates.
         *
         * - In atomic mode: drains the MPSC queue for updated enum entries.
         * - In non-atomic mode: locks the set of updated names and applies them.
         *
         * @param force If true, all settings are updated regardless of change tracking.
         */
        template<bool force = false>
        void update_settings() {
            if constexpr(force) {
                for(auto& fn : setting_updaters) {
                    fn(_store);
                }
                return;
            }
            
            if constexpr(use_atomic) {
                Variables var;
                // Drain the queue; each pop gives you the enum for an updated setting.
                while(updatedQueue.pop(var)) {
                    // Use the enum directly as an index into your updater array.
                    setting_updaters[static_cast<size_t>(var)](_store);
                }
                
            } else {
                std::lock_guard guard{this->_update_settings_mutex};
                for (auto name : this->_updated_settings) {
                    auto it = std::find(setting_names.begin(), setting_names.end(), name);
                    if (it != setting_names.end()) {
                        auto index = std::distance(setting_names.begin(), it);
                        setting_updaters[index](_store);
                    }
                }
            }
        }
       
        /**
         * @brief Grants external threads access to the settings object.
         *
         * Increments a counter tracking concurrent external accesses. Must be matched with `external_thread_done()`.
         *
         * @return Pointer to the thread-local settings object.
         */
        ThreadObject* get_external_ptr() {
#ifndef NDEBUG
            external_access_count.fetch_add(1, std::memory_order_relaxed);
#endif
            return this;
        }
        
        /**
         * @brief Marks completion of external thread access to the settings object.
         *
         * Decrements the external access count. Must be called once per `get_external_ptr()` call.
         */
        void external_thread_done() {
#ifndef NDEBUG
            size_t prev = external_access_count.fetch_sub(1, std::memory_order_relaxed);
            assert(prev > 0 && "External access count underflow: no external thread active to release!");
#endif
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
#ifndef NDEBUG
            // Ensure that no external thread accesses remain when ending the round.
            assert(external_access_count.load(std::memory_order_relaxed) == 0 &&
                   "External thread(s) still active at end of round!");
            if(not _round_lock)
                throw RuntimeError("Cannot end a round. No round was started.");
            _round_lock = false;
#endif
            // (Release round resources if needed)
        }
    };
    
private:
    /**
     * @brief Thread-local singleton instance of settings with callback-based update tracking.
     *
     * This thread-local `object` is created once per thread and holds the full configuration state,
     * including all settings and their update logic. It registers callbacks with the global
     * settings system to be notified when any setting changes. These changes are collected either
     * through an atomic MPSC queue (for lock-free update tracking) or a mutex-protected set,
     * depending on the `use_atomic` flag.
     *
     * On first access, the shared pointer is initialized and:
     * - Registers callbacks for all settings
     * - Wraps the callback in logic that queues the updated setting name
     * - Immediately applies all settings (`update_settings<true>()`)
     */
    inline static thread_local std::shared_ptr<ThreadObject> object{[](){
        auto ptr = std::make_shared<ThreadObject>();
        
        ptr->_callback_collection = GlobalSettings::register_callbacks<cmn::sprite::RegisterInit::DONT_TRIGGER>(
         ptr->setting_names,
         [wptr = std::weak_ptr(ptr)](auto name) {
            std::shared_ptr<ThreadObject> lock = wptr.lock();
            if(not lock)
                return;
            
            if constexpr(use_atomic) {
                auto var = lock->get_enum_name(name);
                lock->updatedQueue.push(var);
                
            } else {
                std::lock_guard guard{lock->_update_settings_mutex};
                lock->_updated_settings.insert(name);
            }
         });
        
        ptr->template update_settings<true>();
        return ptr;
    }()};
    
    RBSettings() = default;
    
public:
    /**
     * @brief RAII helper to begin and end a settings round.
     *
     * Automatically calls start_round() on construction and end_round() on destruction.
     */
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
    };
    
    /**
     * @brief Returns reference to a setting by enum key.
     *
     * @tparam key Enum identifier for the setting.
     */
    template<ThreadObject::Variables key>
    static const auto& get() {
        auto &settings = *object;
        assert(settings._thread_id == std::this_thread::get_id());
        return settings.template get<key>();
    }
    /**
     * @brief Returns reference to a setting by fixed string key.
     *
     * @tparam key Compile-time fixed string name of the setting.
     */
    template<ct::utils::fixed_string key>
    static const auto& get() {
        auto &settings = *object;
        assert(settings._thread_id == std::this_thread::get_id());
        return settings.template get<key>();
    }
    
    static void start_round() {
        object->start_round();
    }
    static void end_round() {
        object->end_round();
    }
    
    static Guard round() {
        return Guard{object.get()};
    }
    
    static ThreadObject* current() {
        return object.get();
    }
};

#define RBS(NAME) round.settings->get< RB_t::ThreadObject::Variables:: NAME >()
#define RBSTR(NAME) round.settings->get< #NAME >()

}
