/**
 * Project Untitled
 */


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
        LOAD,
        STARTUP,
        SYSTEM
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
        
    public:
        GlobalSettings();

        /**
         * Destructor of @class GlobalSettings.
         */
        ~GlobalSettings();
        
        static float& invalid() {
            static float _invalid = infinity<float>();
            return _invalid;
        }

        static void set_invalid(float v) {
            invalid() = v;
        }

        static bool is_invalid(float v) {
            return v == invalid();
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
        static const sprite::Map& defaults();
        static sprite::Map& current_defaults();
        static sprite::Map& current_defaults_with_config();
        static sprite::Map& set_defaults();
        static void set_current_defaults(sprite::Map&&);
        static void set_current_defaults_with_config(sprite::Map&&);
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
