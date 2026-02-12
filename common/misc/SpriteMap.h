#pragma once

#include <commons.pc.h>

namespace cmn {

template <typename T>
concept ParserAvailable = requires(const T& t) {
    { cmn::ParseValue<>::parse_value(t) }
        -> std::same_as<std::string>;
};

namespace sprite {

    class Map;
    class PropertyType;
    
    template<typename T>
    class Property;
    
    class ConstReference {
    private:
        std::weak_ptr<const PropertyType> _type;
        const Map* _container;
        
    public:
        ConstReference(const Map& container, std::weak_ptr<const PropertyType> type)
        : _type(type), _container(&container)//, _name(name)
        { }
        
        ConstReference(const Map& container)
        : _container(&container)//, _name(name)
        { }
        
        bool operator==(const PropertyType& other) const;
        bool operator==(const ConstReference& other) const { return operator==(other.get()); }
        
        bool operator!=(const PropertyType& other) const;
        bool operator!=(const ConstReference& other) const { return operator!=(other.get()); }
        
        template<typename T>
        const Property<T>& toProperty() const;
        
        template<typename T>
            requires (not std::same_as<T, bool>)
        operator Property<T>&() const {
            return toProperty<T>();
        }
        
        template<typename T>
            requires (not std::same_as<T, bool>)
        operator const T() const;
        
        template<typename T>
        auto value() const {
            return toProperty<T>().value();
        }
        
        template<typename T>
        bool is_type() const;
        
        bool valid() const;
        std::string_view type_name() const;
        
        
        std::shared_ptr<const PropertyType> operator *() const {
            if(auto ptr = _type.lock();
               not ptr)
            {
                throw std::runtime_error("Property is not valid.");
            } else
                return ptr;
        }
        std::shared_ptr<const PropertyType> operator ->() const {
            return this->operator*();
        }
        const PropertyType& get() const {
            if(auto ptr = _type.lock();
               not ptr)
            {
                throw std::runtime_error("Property is not valid.");
            } else
                return *ptr;
        }
        const Map& container() const { return *_container; }
        
        std::string toStr() const;
        std::shared_ptr<const PropertyType> ptr() const {
            return _type.lock();
        }
    };
    
    class Reference {
    private:
        std::weak_ptr<PropertyType> _type;
        Map* _container;
        std::unique_ptr<std::string> _name_storage;
        GETTER(std::string_view, name);
    public:
        template <typename T>
            requires std::is_array_v<std::remove_reference_t<T>>
        Reference(Map& container, std::weak_ptr<PropertyType> type, T&& name)
            : _type(type), _container(&container), _name(name, std::size(name) - 1)
        { }
        
        template <typename T>
            requires (std::is_same_v<T, const char*>
                     || std::is_same_v<std::remove_reference_t<T>, std::string_view>)
                     && (not std::is_array_v<std::remove_reference_t<T>>)
        Reference(Map& container, std::weak_ptr<PropertyType> type, T&& name)
            : _type(type), _container(&container), _name(name)
        { }
        
        template <typename T>
            requires std::is_lvalue_reference_v<T>
                     && (not std::is_same_v<std::remove_reference_t<T>, std::string_view>)
                     && (not std::is_array_v<std::remove_reference_t<T>>)
        Reference(Map& container, std::weak_ptr<PropertyType> type, T&& name)
            : _type(type), _container(&container), _name_storage(std::make_unique<std::string>(name)), _name(*_name_storage)
        { }
        
        template <typename T>
            requires (not std::same_as<T, std::weak_ptr<PropertyType>>)
        Reference(Map& container, T&& name) : Reference(container, std::weak_ptr<PropertyType>{}, std::forward<T>(name)) { }

        // Delete the constructor for r-value std::string
        Reference(Map&, std::weak_ptr<PropertyType>, std::string&&) = delete;
        
        bool operator==(const PropertyType& other) const;
        bool operator==(const Reference& other) const { return operator==(other.get()); }
        
        bool operator!=(const PropertyType& other) const;
        bool operator!=(const Reference& other) const { return operator!=(other.get()); }
        
        template<typename T>
        Property<T>& toProperty() const;
        
        template<typename T>
            requires (not std::same_as<T, bool>)
        operator Property<T>&() const {
            return toProperty<T>();
        }
        
        template<typename T>
            requires (not std::same_as<T, bool>)
        operator const T();
        
        template<typename T>
        T value() const {
            return toProperty<T>().value();
        }

		template<typename T>
		void value(const T& v) {
			operator=(v);
		}
        
        template<typename T>
        bool is_type() const;
        
        bool valid() const;
        PropertyType& get() const {
            if(auto ptr = _type.lock();
               ptr != nullptr)
            {
                return *ptr;
            }
            throw std::runtime_error("Property "+std::string(_name)+" does not exist.");
        }
        std::string_view type_name() const;
        
        Map& container() const { return *_container; }

		double speed() const;
		Reference& speed(double s);
        
        template<typename T>
            requires (not std::convertible_to<T, const char*>)
        void operator=(const T& value);
        
        template<typename T>
            requires (std::convertible_to<T, const char*>)
        void operator=(const T& value);
        
        std::string toStr() const;
        std::shared_ptr<PropertyType> ptr() const {
            return _type.lock();
        }
    };

}
}

#include "SpriteProperty.h"

namespace cmn {
namespace sprite {
    /*struct PNameRef {
        PNameRef(const std::string& name);
        PNameRef(const std::string_view& name);
        PNameRef(const std::shared_ptr<PropertyType>& ref);
        
        std::shared_ptr<PropertyType> _ref;
        
        const std::string& name() const;
        
        bool operator==(const PNameRef& other) const;
        bool operator==(const std::string& other) const;
        bool operator<(const PNameRef& other) const;
    };*/
}
}

namespace std
{
    /*template <>
    struct hash<cmn::sprite::PNameRef>
    {
        size_t operator()(const cmn::sprite::PNameRef& k) const;
    };*/
}

namespace cmn {
namespace sprite {

enum class RegisterInit {
    DONT_TRIGGER,
    DO_TRIGGER
};

template <typename T>
concept Iterable = requires(T obj) {
    { std::begin(obj) } -> std::input_iterator;
    { std::end(obj) } -> std::input_iterator;
};

    class Map {
    public:
        enum class Signal {
            NONE,
            EXIT
        };
        
        typedef std::shared_ptr<PropertyType> Store;
        
    protected:
        robin_hood::unordered_flat_map<std::string, Store, MultiStringHash, MultiStringEqual> _props;

        std::atomic<std::size_t> _id_counter{0u};
        robin_hood::unordered_flat_map<std::size_t, std::function<void(Map*)>> _shutdown_callbacks;
        
        // queued‑callback support
        struct PendingCallback {
            std::function<void(std::string_view)>        fn;
            RegisterInit                            init_type;
            std::shared_ptr<std::promise<cmn::CallbackCollection>>     prom;
            std::shared_ptr<std::atomic<int>>       remaining;
            cmn::CallbackCollection addition;
        };
        std::shared_mutex pending_mutex;
        std::unordered_map<std::string, std::vector<PendingCallback>, MultiStringHash, MultiStringEqual> _pending_callbacks;
        
        mutable LOGGED_MUTEX_VAR(_mutex, "sprite::Lock");
        GETTER_I(bool, print_by_default, false);
        
    public:
        auto& mutex() const { return _mutex; }
        
        void do_print(std::string_view name, bool value) {
            auto guard = LOGGED_LOCK(mutex());
            if (auto it = _props.find(name);
                it != _props.end())
            {
                it->second->set_do_print(value);
            }
        }

        void set_print_by_default(bool value) {
            auto guard = LOGGED_LOCK(mutex());
            if (_print_by_default != value) {
                _print_by_default = value;

                for(auto &[key, v] : _props)
                    v->set_do_print(value);
            }
		}

        /**
         * @brief Registers a shutdown callback function.
         *
         * @param callback The callback function to register.
         * @return The ID of the registered callback.
         */
        std::size_t register_shutdown_callback(std::function<void(Map*)> callback) {
            std::size_t id = _id_counter++;
            _shutdown_callbacks[id] = callback;
            return id;
        }

        /**
         * @brief Unregisters a shutdown callback with the given ID.
         *
         * @param id The ID of the shutdown callback to unregister.
         */
        void unregister_shutdown_callback(std::size_t id) {
            auto it = _shutdown_callbacks.find(id);
            if(it != _shutdown_callbacks.end())
                _shutdown_callbacks.erase(it);
        }

        
        friend PropertyType;
        
    public:
        Map() = default;
        
        template<typename... Ts>
            requires (sizeof...(Ts) % 2 == 0 && sizeof...(Ts) > 0)
        Map(Ts... ts) {
            insert_helper(std::forward<Ts>(ts)...);
        }
        
    private:
        template<typename Key, typename Value, typename... Ts>
        void insert_helper(Key key, Value value, Ts... ts) {
            operator[](std::forward<Key>(key)) = std::forward<Value>(value);
            if constexpr(sizeof...(Ts) > 0) {
                insert_helper(std::forward<Ts>(ts)...);
            }
        }
        
    public:
        Map(const Map& other);
        Map(Map&& other);
        Map& operator=(const Map& other);
        Map& operator=(Map&& other);
        
        ~Map();
        
        bool empty() const {
            auto guard = LOGGED_LOCK(mutex());
            return _props.empty();
        }
        size_t size() const {
            auto guard = LOGGED_LOCK(mutex());
            return _props.size();
        }
        
        bool operator==(const Map& other) const {
            if(other.size() != size())
                return false;
            
            auto guard = LOGGED_LOCK(mutex());
            auto guard2 = LOGGED_LOCK(other.mutex());
            //std::scoped_lock guard(mutex(), other.mutex());
            auto it0 = other._props.begin();
            auto it1 = _props.begin();
            
            for(; it0 != other._props.end(); ++it0, ++it1) {
                if(!(it0->first == it1->first) || !(*it0->second == *it1->second)) {
                    Print(it0->first," != ",it1->first," || ");
                    return false;
                }
            }
            
            return true;
        }
        
        template<RegisterInit init_type = RegisterInit::DO_TRIGGER, typename Callback, typename Str>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
                    && std::convertible_to<Str, std::string>
        CallbackFuture register_callbacks(std::initializer_list<Str> names, Callback callback) {
            if constexpr(std::same_as<Str, const char*>) {
                std::vector<std::string_view> converted_names;
                for(const char* str : names) {
                    converted_names.emplace_back(str);
                }
                return register_callbacks_impl<init_type>(converted_names, callback);
            } else
                return register_callbacks_impl<init_type>(names, callback);
        }
        
        template<RegisterInit init_type = RegisterInit::DO_TRIGGER, typename Callback, typename Str, size_t N>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
            && (std::convertible_to<Str, std::string> || std::constructible_from<std::string, Str>)
        CallbackFuture register_callbacks(const std::array<Str, N>& names, Callback callback) {
            if constexpr(std::same_as<Str, const char*>) {
                std::vector<std::string_view> converted_names;
                for(const char* str : names) {
                    converted_names.emplace_back(str);
                }
                return register_callbacks_impl<init_type>(converted_names, callback);
            } else
                return register_callbacks_impl<init_type>(names, callback);
        }
        
        template<RegisterInit init_type = RegisterInit::DO_TRIGGER, typename Callback, typename Str>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
                    && std::convertible_to<Str, std::string>
        CallbackFuture register_callbacks(const std::vector<Str>& names, Callback callback) {
            return register_callbacks_impl<init_type>(names, callback);
        }
        
        template<RegisterInit init_type = RegisterInit::DO_TRIGGER, std::ranges::range Container, typename Callback>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
        CallbackFuture register_callbacks_impl(const Container& names, Callback callback) {
            CallbackFuture future;
            
            auto remaining = std::make_shared<std::atomic<int>>(0);
            auto prom      = std::make_shared<std::promise<cmn::CallbackCollection>>();
            
            for(const auto& name : names) {
                if(has(name)) {
                    future.collection._ids[std::string(name)] = operator[](name).get().registerCallback(callback);
                    
                } else {
                    std::unique_lock g{pending_mutex};
                    _pending_callbacks[std::string(name)].emplace_back(
                        callback, init_type, prom, remaining, cmn::CallbackCollection{}
                    );
                    ++(*remaining);
#ifndef NDEBUG
                    FormatWarning("Cannot find property ", name, " to attach a callback to. Waiting for it to appear.");
#endif
                }
            }
            
            if(*remaining == 0)   // no pending callbacks
                prom->set_value({});
            else
                future.ready = prom->get_future();
            
            if constexpr(init_type == RegisterInit::DO_TRIGGER) {
                trigger_callbacks(future.collection);
            }
            
            return future;
        }
        
        void unregister_callbacks(CallbackFuture&& future) {
            if(not future.is_ready()) {
                FormatWarning("Future is not ready when destroyed.");
                //future.wait();
            } else {
                future.finish();
            }
            unregister_callbacks(std::move(future.collection));
        }
        
        void unregister_callbacks(CallbackCollection&& collection) {
            auto guard = LOGGED_LOCK(mutex());
            for(auto &[name, id] : collection._ids) {
                auto it = _props.find(name);
                if(it != _props.end()) {
                    it->second->unregisterCallback(id);
                } else
                    FormatWarning("Cannot find property ", name, " from callback holder.");
            }
            collection._ids.clear();
        }
        
        void trigger_callbacks(const CallbackCollection& collection) {
            for(auto &[name, id] : collection._ids) {
                operator[](name).get().triggerCallback(id);
            }
        }
        
        template<typename T>
            requires std::is_same_v<T, const char*>
                    || std::is_lvalue_reference_v<T&&>
                    || std::is_array_v<std::remove_reference_t<T>>
        ConstReference at(T&& name) const {
            return operator[](std::forward<T>(name));
        }
        
        template<typename T>
            requires std::is_same_v<T, const char*>
                    || std::is_lvalue_reference_v<T&&>
                    || std::is_same_v<std::remove_reference_t<T>, std::string_view>
                    || std::is_array_v<std::remove_reference_t<T>>
        Reference operator[](T&& name) {
            auto guard = LOGGED_LOCK(mutex());
            decltype(_props)::const_iterator it;

            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*>
                         || std::is_array_v<std::remove_reference_t<T>>
                         || std::same_as<std::remove_reference_t<T>, std::string_view>)
            {
                if constexpr(std::same_as<std::remove_reference_t<T>, std::string_view>)
                    it = _props.find(name);
                else if constexpr(std::is_array_v<std::remove_reference_t<T>>)
                    it = _props.find(std::string_view(name, std::size(name) - 1));  // -1 to remove null terminator
                else
                    it = _props.find(std::string_view(name));
                
                /// we can keep this here as a string reference, because we
                /// are using const char[35] which do not get deallocated
                if(it != _props.end())
                    return Reference(*this, it->second, name);
                
            } else {
                it = _props.find(name);
                
                /// we have to make a copy of name here...
                /// we cant be sure this exists later still
                if(it != _props.end())
                    return Reference(*this, it->second, it->first);
            }

            return Reference(*this, name);
        }
        
        template<typename T>
            requires std::is_same_v<T, const char*>
                    || std::is_lvalue_reference_v<T&&>
                    || std::is_array_v<std::remove_reference_t<T>>
        ConstReference operator[](T&& name) const {
            auto guard = LOGGED_LOCK(mutex());
            decltype(_props)::const_iterator it;

            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*>) {
                it = _props.find(std::string_view(name));
            } else if constexpr(std::is_array_v<std::remove_reference_t<T>>) {
                it = _props.find(std::string_view(name, std::size(name) - 1));  // -1 to remove null terminator
            } else {
                it = _props.find(name);
            }

            if(it != _props.end())
                return ConstReference(*this, it->second);

            return ConstReference(*this);
        }
        
        bool has(std::string_view name) const {
            auto guard = LOGGED_LOCK(mutex());
            return _props.contains(name);
        }
        
        template<typename T>
        bool is_type(const std::string_view& name, const T&) const {
            return is_type<T>(name);
        }
        
        template<typename T>
        bool is_type(const std::string_view& name) const {
            return has(name) && operator[](name).is_type<T>();
        }
        
        template<typename T>
        Property<T>& set(const std::string_view& name, const T& value) {
            operator[](name) = value;
        }
        
        template<typename T>
        Property<T>& insert(const std::string_view& name, const T& value) {
            if(has(name)) {
                std::string e = "Property already "+(std::string)name+" already exists.";
                FormatError(e.c_str());
                throw PropertyException(e);
            }
            
            auto property_ = new Property<T>(name, value);
            {
                auto ptr = Store(property_);
                if (print_by_default()) {
                    ptr->set_do_print(true);
                    if constexpr(ParserAvailable<T>)
                    {
                        Print(no_quotes(ptr->name()), "<", no_quotes(Meta::name<T>()), "> = ", value);
                    } else
                        Print(no_quotes(ptr->name()), "<", no_quotes(type_name<T>()), "> added.");
                }
                
                auto guard = LOGGED_LOCK(mutex());
                _props[ptr->name()] = std::move(ptr);
            }
            
            /// collect the callbacks that need to be called
            std::vector<std::size_t> indexes;
            
            std::unique_lock g{pending_mutex};
            if(auto it = _pending_callbacks.find(name);
               it != _pending_callbacks.end())
            {
                const auto str = std::string(name);
#ifndef NDEBUG
                Print("Attaching callbacks for ", name, " now that it has been inserted.");
#endif
                
                for(auto &future : it->second) {
                    auto cb = property_->registerCallback(future.fn);
                    future.addition._ids[str] = cb;
                    
                    --(*future.remaining);
                    if(*future.remaining == 0)  { // no pending callbacks
                        future.prom->set_value(std::move(future.addition));
                    }
                    
                    if (future.init_type == RegisterInit::DO_TRIGGER) {
                        indexes.push_back(cb);
                    }
                }
                
                _pending_callbacks.erase(it);
            }
            
            for(auto cb : indexes)
                property_->triggerCallback(cb);
            
            return *property_;
        }
        
        void erase(std::string_view key) {
            auto guard = LOGGED_LOCK(mutex());
            auto it = _props.find(key);
            if(it != _props.end()) {
                _props.erase(it);
            } else {
                std::string e = "Map does not have key '"+(std::string)key+"'.";
                FormatError(e.c_str());
                throw PropertyException(e);
            }
        }
        
        std::vector<std::string> keys() const {
            auto guard = LOGGED_LOCK(mutex());
            std::vector<std::string> result;
            result.reserve(_props.size());

            for (auto &p: _props)
                result.push_back(p.first);

            std::sort(result.begin(), result.end());
            return result;
        }
        
        std::string toStr() const {
            return "Map<size:" + Meta::toStr(size()) + ">";
        }
    };
    
    template<typename T>
    Property<T>& Reference::toProperty() const {
        if(auto ptr = _type.lock();
           not ptr)
        {
            throw U_EXCEPTION("Property ",name()," is invalid.");
        } else
            return ptr->toProperty<T>();
    }
    
    template<typename T>
        requires (not std::same_as<T, bool>)
    Reference::operator const T() {
        auto ptr = _type.lock();
        Property<T> *tmp = dynamic_cast<Property<T>*>(ptr.get());
        if (tmp) {
            return tmp->value();
        }
        
        std::string e = not ptr || not ptr->valid()
            ? "Cannot find variable '" + (std::string)_name + "' of type " + Meta::name<T>() + " in map."
            : "Cannot cast '" + (std::string)_name + "' of type " + (ptr->valid() ? (std::string)ptr->type_name() : "<variable not found>") + " to "+ Meta::name<T>() +".";
        FormatError(e.c_str());
        throw PropertyException(e);
    }
    
    template<typename T>
    bool Reference::is_type() const {
        if(auto ptr = _type.lock();
           not ptr)
        {
            return false;
        } else
            return dynamic_cast<const Property<T>*>(ptr.get()) != nullptr;
    }
    
template<typename T>
    requires (not std::convertible_to<T, const char*>)
void Reference::operator=(const T& value) {
    if (auto ptr = _type.lock();
        ptr /*&& ptr->valid()*/)
    {
        ptr->operator=(value);
    }
    else {
        _container->insert(_name, value);
    }
}

template<typename T>
    requires (std::convertible_to<T, const char*>)
void Reference::operator=(const T& value) {
    if (auto ptr = _type.lock();
        ptr && ptr->valid())
    {
        ptr->operator=(std::string(value));
    }
    else {
        _container->insert(_name, std::string(value));
    }
}

    template<typename T>
    const Property<T>& ConstReference::toProperty() const {
        auto ptr = _type.lock();
        if(not ptr)
            throw U_EXCEPTION("Invalid property.");
        return ptr->toProperty<T>();
    }
    
    template<typename T>
        requires (not std::same_as<T, bool>)
    ConstReference::operator const T() const {
        auto ptr = _type.lock();
        const Property<T> *tmp = dynamic_cast<const Property<T>*>(ptr.get());
        if (tmp) {
            return tmp->value();
        }
        
        std::string e = not ptr || not ptr->valid()
            ? "Cannot find variable of type " + Meta::name<T>() + " in map."
            : "Cannot cast variable '"+(std::string)ptr->name()+"' of type " + (ptr->valid() ? (std::string)ptr->type_name() : "<variable not found>") + " to "+ Meta::name<T>() +".";
        FormatError(e.c_str());
        throw PropertyException(e);
    }
    
    template<typename T>
    bool ConstReference::is_type() const {
        if(auto ptr = _type.lock();
           not ptr)
        {
            return false;
        } else {
            return dynamic_cast<const Property<T>*>(ptr.get()) != nullptr;
        }
    }

    template<typename T>
    void PropertyType::operator=(const T& value) {
        // First check if this is an optional type being assigned from the inner type
        // This handles the case where Property<std::optional<T>> is assigned a value of type T
        // Use type_index comparison instead of dynamic_cast for better performance
        if(is_type<std::optional<T>>()) {
            // Type is confirmed to be Property<std::optional<T>>, use static_cast
            auto optional_ptr = static_cast<Property<std::optional<T>>*>(this);
            // Assign the value wrapped in an optional
            *optional_ptr = std::optional<T>(value);
            return;
        }
        
        // Not an optional type, proceed with normal assignment
#ifndef NDEBUG
        auto ptr = dynamic_cast<Property<T>*>(this);
        if(not ptr) {
            throw InvalidArgumentException("Cannot cast property ", name(), " to ", Meta::name<T>(), " because it is ", type_name(),".");
        }
        
        if(ptr->valid()) {
            *ptr = value;
            
        } else {
            std::stringstream ss;
            ss << "Reference to "+toStr()+" cannot be cast to type of ";
            ss << Meta::name<T>();
            FormatError(ss.str().c_str());
            throw PropertyException(ss.str());
        }
#else
        /// do not pretend as if we had any choice here. we cant tell
        /// if it worked or not...
        auto ptr = static_cast<Property<T>*>(this);
        *ptr = value;
#endif
        
    }

    template<typename T>
    void Property<T>::value(const T& v) {
        if(not assign_if(v))
            return;
        
        if(_do_print) {
            if constexpr(ParserAvailable<T>)
            {
                Print(no_quotes(name()), "<", no_quotes(Meta::name<T>()), "> = ", no_quotes(utils::ShortenText(Meta::toStr(v), 1000)));
            } else {
                Print(no_quotes(name()), "<", no_quotes(cmn::type_name<T>()), "> updated.");
            }
        }
        
        triggerCallbacks();
    }

    template<typename T>
    void Property<T>::copy_to(Map& other) const {
        auto name = std::string_view{_name};
        if(other.is_type<T>(name)) {
            if constexpr(trivial)
                other[name] = _value.load().value();
            else {
                std::unique_lock guard(_property_mutex);
                other[name] = _value.value();
            }
            return;
            
        } else if(other.has(name))
            other.erase(name);

        if constexpr(trivial) {
            other.insert(name, _value.load().value());
        } else {
            std::unique_lock guard(_property_mutex);
            other.insert(name, _value.value());
        }
    }
}
}

