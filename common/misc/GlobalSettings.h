#ifndef _GLOBALSETTINGS_H
#define _GLOBALSETTINGS_H

#include <commons.pc.h>
#include <misc/SpriteMap.h>

#ifdef _WIN32
#undef max
#undef min
#endif

#define SETTING(NAME) (GlobalSettings::get(#NAME))

namespace cmn {
    /*namespace detail {
        struct g_GSettingsSingletonStruct;
    }*/
    
    ENUM_CLASS(AccessLevelType,
        PUBLIC,
        INIT,
        LOAD,
        STARTUP,
        SYSTEM
    )
    
    ENUM_CLASS(SeasonType,
        SPRING,
        SUMMER,
        AUTUMN,
        WINTER,
        EASTER,
        CHRISTMAS,
        HALLOWEEN,
        STAR_WARS_DAY,
        PRIDE_MONTH,
        NEW_YEAR
    )
    
    typedef AccessLevelType::Class AccessLevel;
    
    /**
     * @class GlobalSettings
     */
    class GlobalSettings {
    public:
        typedef std::unordered_map<std::string, std::string> docs_map_t;
        typedef std::unordered_map<std::string, AccessLevel, MultiStringHash, MultiStringEqual> user_access_map_t;
        
    private:
        
        /**
         * A map that contains all the settings.
         */
        sprite::Map _map, _defaults, _current_defaults, _current_defaults_with_config;
        
        /**
         * A map that contains all available documentation for settings.
         */
        docs_map_t _doc;
        
        /**
         * A map specifiying the access level needed to be able to change
         * a parameter. Access levels are defined in
         */
        user_access_map_t _access_levels;
        
        //friend struct detail::g_GSettingsSingletonStruct;
        static std::mutex& mutex();
        static std::mutex& defaults_mutex();
        
    public:
        GlobalSettings();

        /**
         * Destructor of @class GlobalSettings.
         */
        ~GlobalSettings();
        
    private:
        static std::atomic<Float2_t>& invalid_raw() {
            static std::atomic<Float2_t> _invalid{ infinity<Float2_t>()
            };
            return _invalid;
        }
        
    public:
        static Float2_t invalid() {
            return invalid_raw().load();
        }
        
        static constexpr bool is_invalid_inf(Float2_t v) {
            return cmn::isinf(v);
        }
        static constexpr bool is_invalid_nan(Float2_t v) {
            return cmn::isnan(v);
        }
        static bool is_invalid_(Float2_t v) {
            return v == invalid();
        }

        static void set_invalid(Float2_t v) {
            if(std::isnan(v)) {
                set_is_invalid_fn(is_invalid_nan);
            } else if(std::isinf(v)) {
                set_is_invalid_fn(is_invalid_inf);
            } else
                set_is_invalid_fn(is_invalid_);
            
            invalid_raw() = v;
        }

        static auto& is_invalid_mutex() {
            static std::shared_mutex mutex;
            return mutex;
        }
        static auto& is_invalid_fn() {
            static std::function<bool(Float2_t)> _is_invalid = is_invalid_inf;
            return _is_invalid;
        }
        
        static bool is_invalid(float v) {
            std::shared_lock guard(is_invalid_mutex());
            return is_invalid_fn()(v);
        }
        
        static void set_is_invalid_fn(auto&& fn) {
            std::unique_lock guard(is_invalid_mutex());
            is_invalid_fn() = std::move(fn);
        }
        
        //! return the instance
        static GlobalSettings* instance(GlobalSettings* ptr = nullptr) {
            static std::mutex _mutex;
            static GlobalSettings* _instance{new GlobalSettings()};
            
            std::unique_lock guard(_mutex);
            if(ptr)
                _instance = ptr;
            return _instance;
        }
        
        static void set_instance(GlobalSettings*);
        
        /**
         * Returns a reference to the settings map.
         */
        static sprite::Map& map();
        static std::shared_ptr<const sprite::PropertyType> at(std::string_view);
        static const sprite::Map& defaults();
        static std::optional<sprite::Map> defaults(std::string_view name);
        static std::optional<sprite::Map> current_defaults(std::string_view);
        static void current_defaults(std::string_view, const sprite::Map&);
        static sprite::Map get_current_defaults();
        static sprite::Map& current_defaults_with_config();
        static sprite::Map& set_defaults();
        static void set_current_defaults(const sprite::Map&);
        static void set_current_defaults_with_config(const sprite::Map&);
        static docs_map_t& docs();
        
        static bool is_runtime_quiet(const sprite::Map* = nullptr);
        
        //! Returns true if the given key exists.
        static bool has(const std::string& name);
        
        //! Returns true if documentation for the given key exists.
        static bool has_doc(const std::string& name);
        
        //! Returns true if this key may be modified by the user.
        static bool has_access(const std::string& name, AccessLevel level);
        static AccessLevel access_level(const std::string_view& name);
        static void set_access_level(const std::string& name, AccessLevel access_level);
        
        static user_access_map_t& access_levels();
        
        /**
         * @param name
         */
        static sprite::Reference _get(const char* name);
        
        template<typename T>
            requires std::is_same_v<std::remove_reference_t<T>, const char*>
                        || std::is_lvalue_reference_v<T&&>
                        || std::is_array_v<std::remove_reference_t<T>>
        static sprite::Reference get(T&& name) {
            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*> || std::is_array_v<std::remove_reference_t<T>>)
                return _get(name);
            else
                return _get(name.c_str());
        }
        
        /**
         * Retrieves documentation for a given name.
         * @param name Name of the setting.
         */
        static const std::string& doc(const std::string& name);
        
        /**
         * Returns reference. Creates object if it doesnt exist.
         * @param name
         */
        template<typename T>
        static sprite::Reference get_create(const std::string& name, const T& default_value) {
            std::lock_guard<std::mutex> lock(mutex());
            
            if(map().has(name)) {
                return map()[name];
                
            } else {
                auto &p = map().insert(name, default_value);
                return sprite::Reference(map(), p);
            }
        }
        
        /**
         * Loads parameters from a file.
         * @param filename Name of the file
         */
        static std::map<std::string, std::string> load_from_file(const std::map<std::string, std::string>& deprecations, const std::string& filename, AccessLevel access, const std::vector<std::string>& exclude = {}, sprite::Map* = nullptr, const sprite::Map* additional = nullptr);
        
        /**
         * Loads parameters from a string.
         * @param str the string
         */
        static std::map<std::string, std::string> load_from_string(sprite::MapSource, const std::map<std::string, std::string>& deprecations, sprite::Map& map, const std::string& str, AccessLevel access, bool correct_deprecations = false, const std::vector<std::string>& exclude = {}, const sprite::Map* additional = nullptr);

        /**
         * Returns the current season based on the local date.
         */
        static SeasonType::Class currentSeason();
        static std::pair<int,int> calculateEasterDate(int year);
    };
    
    /*namespace detail {
        struct g_GSettingsSingletonStruct {
            GlobalSettings g_GlobalSettingsInstance;
        };
        
        extern g_GSettingsSingletonStruct g_GSettingsSingleton;
    }*/

    /**
     * Combines all required maps into a joint object.
     */
    struct SettingsMaps {
        sprite::Map map;
        GlobalSettings::docs_map_t docs;
        GlobalSettings::user_access_map_t access_levels;
    };
}

#endif //_GLOBALSETTINGS_H
