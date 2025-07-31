#ifndef _GLOBALSETTINGS_H
#define _GLOBALSETTINGS_H

#include <commons.pc.h>
#include <misc/SpriteMap.h>
#include <misc/Deprecations.h>

#ifdef _WIN32
#undef max
#undef min
#endif

#define SETTING(NAME) (cmn::GlobalSettings::get(#NAME))
#define BOOL_SETTING(NAME) (cmn::GlobalSettings::read_value_with_default(#NAME, false))
#define OPTIONAL_SETTING(NAME, ...) (cmn::GlobalSettings::read_value< __VA_ARGS__ >( #NAME ))
#define READ_SETTING(NAME, ...) (cmn::GlobalSettings::read_value< __VA_ARGS__ >( #NAME ).value())
#define READ_SETTING_WITH_DEFAULT(NAME, ...) (cmn::GlobalSettings::read_value_with_default( #NAME , __VA_ARGS__ ))

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
    
    using AccessLevel = AccessLevelType::Class;
    using docs_map_t = std::unordered_map<std::string, std::string, MultiStringHash, MultiStringEqual>;
    using user_access_map_t = std::unordered_map<std::string, AccessLevel, MultiStringHash, MultiStringEqual>;

    using NoType = std::nullopt_t;

    struct Configuration {
        cmn::sprite::Map values, examples, defaults;
        docs_map_t docs;
        user_access_map_t access;
        
        void set_access(std::string_view name, cmn::AccessLevel w)
        {
            assert(values.has(name));
            access[(std::string)name] = w;
        }
        std::optional<AccessLevel> access_level(std::string_view name) const noexcept
        {
            if(auto it = access.find(name);
               it != access.end())
            {
                return it->second;
            }
            return std::nullopt;
        }
        AccessLevel _access_level(std::string_view name) const noexcept
        {
            if(auto it = access.find(name);
               it != access.end())
            {
                return it->second;
            }
            return AccessLevelType::PUBLIC;
        }
        
        
        std::optional<sprite::ConstReference> get_default(std::string_view name) const
        {
            auto value = defaults.at(name);
            if(value.valid()) {
                return value;
            }
            return std::nullopt;
        }
        
        bool has(std::string_view name) const noexcept {
            return values.has(name);
        }
        bool has_example(std::string_view name) const {
            return examples.has(name);
        }
        
        auto at(cmn::StringLike auto&& name) const {
            return values.at(name);
        }
        auto def(cmn::StringLike auto&& name) const {
            return defaults.at(name);
        }
    };
    
    /**
     * @class GlobalSettings
     */
    class GlobalSettings {
    public:
        
        /**
         * Configuration options for loading settings.
         */
        struct LoadOptions {
            sprite::MapSource source = sprite::MapSource("<none>");
            Deprecations deprecations;
            AccessLevel access = AccessLevelType::PUBLIC;
            bool correct_deprecations = true;
            std::vector<std::string> exclude = {};
            sprite::Map* target = nullptr;
            const sprite::Map* additional = nullptr;
        };
        
    protected:
        Configuration _config;
        
        /**
         * A map that contains all the settings.
         */
        sprite::Map _current_defaults, _current_defaults_with_config;
        
        //friend struct detail::g_GSettingsSingletonStruct;
        static std::shared_mutex& mutex();
        static std::shared_mutex& defaults_mutex();
        
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
        
        static inline thread_local bool is_reading_config{false};
        static inline thread_local bool is_writing_config{false};
        
        enum class Access {
            CONFIGURATION,
            CURRENT
        };
        
        struct ReadG {
            ReadG() {
                is_reading_config = true;
            }
            ~ReadG() {
                is_reading_config = false;
            }
        };
        struct WriteG {
            WriteG() {
                is_reading_config = true;
                is_writing_config = true;
            }
            ~WriteG() {
                is_writing_config = false;
                is_reading_config = false;
            }
        };
        
        template<typename Fn>
        static auto write(Fn&& fn) {
            static constexpr Access access = std::invocable<Fn, Configuration&> ? Access::CONFIGURATION : Access::CURRENT;
            
            if(is_writing_config) {
                if constexpr(access == Access::CONFIGURATION) {
                    return fn(instance()->_config);
                    
                } else if constexpr(access == Access::CURRENT) {
                    static_assert(std::invocable<Fn, sprite::Map&, sprite::Map&>, "We are not accessing a configuration object, so we need to accept the two (current, with_config) maps.");
                    
                    return fn(instance()->_current_defaults, instance()->_current_defaults_with_config);
                    
                } else {
                    static_assert(access == (Access)(size_t)-1, "Unknown access type.");
                }
                //
                
            } else if(is_reading_config) {
                throw RuntimeError("Cannot call write recursively with read.");
            }
            
            /// set reading flag
            WriteG g;
            
            if constexpr(access == Access::CONFIGURATION) {
                std::unique_lock guard{mutex()};
                return fn(instance()->_config);
                
            } else if constexpr(access == Access::CURRENT) {
                static_assert(std::invocable<Fn, sprite::Map&, sprite::Map&>, "We are not accessing a configuration object, so we need to accept the two (current, with_config) maps.");
                
                std::unique_lock guard{defaults_mutex()};
                return fn(instance()->_current_defaults, instance()->_current_defaults_with_config);
                
            } else {
                static_assert(access == (Access)(size_t)-1, "Unknown access type.");
            }
        }
        
        template<typename Fn>
        static auto read(Fn&& fn) {
            static constexpr Access access = std::invocable<Fn, const Configuration&> ? Access::CONFIGURATION : Access::CURRENT;
            
            if(is_reading_config
               || is_writing_config)
            {
                if constexpr(access == Access::CONFIGURATION) {
                    return fn(instance()->_config);
                    
                } else if constexpr(access == Access::CURRENT) {
                    static_assert(std::invocable<Fn, const sprite::Map&, const sprite::Map&>, "We are not accessing a configuration object, so we need to accept the two (current, with_config) maps.");
                    return fn(instance()->_current_defaults, instance()->_current_defaults_with_config);
                    
                } else {
                    static_assert(access == (Access)(size_t)-1, "Unknown access type.");
                }
                
                //throw RuntimeError("Cannot call write recursively.");
            }
            
            /// set reading flag
            ReadG g;
            
            if constexpr(access == Access::CONFIGURATION) {
                std::shared_lock guard{mutex()};
                return fn(instance()->_config);
                
            } else if constexpr(access == Access::CURRENT) {
                static_assert(std::invocable<Fn, const sprite::Map&, const sprite::Map&>, "We are not accessing a configuration object, so we need to accept the two (current, with_config) maps.");
                
                std::shared_lock guard{defaults_mutex()};
                return fn(instance()->_current_defaults, instance()->_current_defaults_with_config);
                
            } else {
                static_assert(access == (Access)(size_t)-1, "Unknown access type.");
            }
        }
        
        /**
         * Returns a reference to the settings map.
         */
        //static Configuration& config();
        //static sprite::Map& map();
        static std::shared_ptr<const sprite::PropertyType> at(std::string_view);
        //static const sprite::Map& defaults();
        //static std::shared_ptr<const sprite::PropertyType> get_default(std::string_view name);
        static std::optional<sprite::Map> current_defaults(std::string_view);
        static void current_defaults(std::string_view, const sprite::Map&);
        //static sprite::Map get_current_defaults();
        //static sprite::Map& current_defaults_with_config();
        //static docs_map_t& docs();
        
        template<typename T>
        static std::optional<T> get_with_type(const sprite::Map& map, std::string_view name) {
            auto v = map.at(name);
            if(v.valid()) {
                return v.value<T>();
            }
            return std::nullopt;
        }
        static sprite::ConstReference read_without_type(const sprite::Map& map, std::string_view name) {
            return map.at(name);
        }
        static sprite::Reference write_without_type(sprite::Map& map, std::string_view name) {
            return map[name];
        }
        
        static auto read_default_from_all(std::string_view name) {
            auto v = read([name](const sprite::Map& current, [[maybe_unused]] const sprite::Map& with_config)
            {
                return read_without_type(current, name);
            });
            
            if(v.valid())
                return v;
            
            return read([name](const Configuration& config) {
                return read_without_type(config.defaults, name);
            });
        }
        
        /**
         * ====================================================
         * Accessors for values, defaults, examples maps
         * ====================================================
         */
        template<typename T>
        static bool is_type(std::string_view name) {
            return read([name](const Configuration& c) {
                auto v = read_without_type(c.values, name);
                return v.valid() ? v->is_type<T>() : false;
            });
        }
        template<typename T>
        static T read_value_with_default(std::string_view name, T default_value) {
            return read([name, &default_value](const Configuration& c) -> T {
                if constexpr(std::same_as<T, NoType>) {
                    auto v = read_without_type(c.values, name);
                    return v.valid() ? v->value<T>() : default_value;
                } else {
                    auto v = get_with_type<T>(c.values, name);
                    return v ? *v : default_value;
                }
            });
        }
        template<typename T>
        static auto read_value(std::string_view name) {
            return read([name](const Configuration& c) {
                if constexpr(std::same_as<T, NoType>)
                    return read_without_type(c.values, name);
                else
                    return get_with_type<T>(c.values, name);
            });
        }
        template<typename T>
        static auto write_value(std::string_view name) {
            return write([name](Configuration& c) {
                if constexpr(std::same_as<T, NoType>)
                    return write_without_type(c.values, name);
                else
                    return get_with_type<T>(c.values, name);
            });
        }
        static bool has_value(std::string_view name) {
            return read([name](const Configuration& c) {
                return c.values.has(name);
            });
        }
        
        template<typename T>
        static auto read_default(std::string_view name) {
            return read([name](const Configuration& c) {
                if constexpr(std::same_as<T, NoType>)
                    return read_without_type(c.defaults, name);
                else
                    return get_with_type<T>(c.defaults, name);
            });
        }
        static bool has_default(std::string_view name) {
            return read([name](const Configuration& c) {
                return c.defaults.has(name);
            });
        }
        
        template<typename T>
        static auto read_current_default(std::string_view name) {
            return read([name](const sprite::Map& current, auto&) {
                if constexpr(std::same_as<T, NoType>)
                    return read_without_type(current, name);
                else
                    return get_with_type<T>(current, name);
            });
        }
        static bool has_current_default(std::string_view name) {
            return read([name](const sprite::Map& current, auto&) {
                return current.has(name);
            });
        }
        
        template<typename T>
        static auto read_current_default_with_config(std::string_view name) {
            return read([name](const sprite::Map&, const sprite::Map& with_config) {
                if constexpr(std::same_as<T, NoType>)
                    return read_without_type(with_config, name);
                else
                    return get_with_type<T>(with_config, name);
            });
        }
        static bool has_current_default_with_config(std::string_view name) {
            return read([name](const sprite::Map&, const sprite::Map& with_config) {
                return with_config.has(name);
            });
        }
        
        template<typename T>
        static auto read_example(std::string_view name) {
            return read([name](const Configuration& c) {
                if constexpr(std::same_as<T, NoType>)
                    return read_without_type(c.examples, name);
                else
                    return get_with_type<T>(c.examples, name);
            });
        }
        static bool has_example(std::string_view name) {
            return read([name](const Configuration& c) {
                return c.examples.has(name);
            });
        }
        
        static std::optional<std::string_view> read_doc(std::string_view name) {
            return read([name](const Configuration& c) -> std::optional<std::string_view> {
                auto it = c.docs.find(name);
                if(it != c.docs.end())
                    return std::string_view(it->second);
                return std::nullopt;
            });
        }
        
        /**
         * ====================================================
         * Some methods related to callbacks on the values map.
         * ====================================================
         */
        
        template<sprite::RegisterInit init = sprite::RegisterInit::DO_TRIGGER, StringLike Str>
        static cmn::CallbackFuture register_callbacks(std::initializer_list<Str> names, auto&& fn) {
            return write([&](Configuration& config){
                return config.values.register_callbacks<init>(std::move(names), std::forward<decltype(fn)>(fn));
            });
        }
        template<sprite::RegisterInit init = sprite::RegisterInit::DO_TRIGGER, StringLike Str>
        static cmn::CallbackFuture register_callbacks(const std::vector<Str>& names, auto&& fn) {
            return write([&](Configuration& config){
                return config.values.register_callbacks<init>(names, std::forward<decltype(fn)>(fn));
            });
        }
        template<sprite::RegisterInit init = sprite::RegisterInit::DO_TRIGGER, StringLike Str, size_t N>
        static cmn::CallbackFuture register_callbacks(const std::array<Str, N>& names, auto&& fn) {
            return write([&](Configuration& config){
                return config.values.register_callbacks<init>(names, std::forward<decltype(fn)>(fn));
            });
        }
        static void unregister_callbacks(cmn::CallbackFuture&& callback) {
            return write([&](Configuration& config){
                config.values.unregister_callbacks(std::move(callback));
            });
        }
        
        static void do_print(std::string_view name, bool value) {
            return write([=](Configuration& config){
                config.values.do_print(name, value);
            });
        }
        
        static auto register_shutdown_callback(auto&& fn) {
            return write([&](Configuration& config){
                return config.values.register_shutdown_callback(std::forward<decltype(fn)>(fn));
            });
        }
        
        static auto keys() {
            return read([](const Configuration& c) {
                return c.values.keys();
            });
        }
        
        static void set_defaults(const sprite::Map&);
        static void set_current_defaults(const sprite::Map&);
        static void set_current_defaults_with_config(const sprite::Map&);
        
        static bool is_runtime_quiet(const sprite::Map* = nullptr);
        
        //! Returns true if this key may be modified by the user.
        static bool has_access(const std::string& name, AccessLevel level);
        static AccessLevel access_level(std::string_view name);
        static void set_access_level(const std::string& name, AccessLevel access_level);
        
        //static user_access_map_t& access_levels();
        
        /**
         * @param name
         */
        static sprite::Reference _get(std::string_view name);
        
        template<typename T>
            requires std::is_same_v<std::remove_reference_t<T>, const char*>
                        || std::is_lvalue_reference_v<T&&>
                        || std::is_array_v<std::remove_reference_t<T>>
        static sprite::Reference get(T&& name) {
            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*>)
                return _get(std::string_view(name));
            else if constexpr(std::is_array_v<std::remove_reference_t<T>>)
                return _get(std::string_view(name, std::size(name) - 1));
            else
                return _get(name);
        }
        
        /**
         * Returns reference. Creates object if it doesnt exist.
         * @param name
         */
        template<typename T>
        static sprite::Reference get_create(std::string_view name, const T& default_value) {
            write([&](Configuration& config) -> sprite::Reference {
                if(config.values.has(name)) {
                    return config.values.at(name);
                }
                
                auto &p = config.values.insert((std::string)name, default_value);
                return sprite::Reference(config.values, p);
            });
        }
        
        /**
         * Loads parameters from a file.
         * @param filename Name of the file
         */
        static std::map<std::string, std::string> load_from_file(
            const std::string& filename,
            LoadOptions options
        );
        
        /**
         * Loads parameters from a string.
         * @param str the string
         */
        static std::map<std::string, std::string> load_from_string(
            const std::string& str,
            LoadOptions options
        );

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
}

#endif //_GLOBALSETTINGS_H
