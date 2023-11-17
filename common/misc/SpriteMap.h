#pragma once

#include <commons.pc.h>

namespace cmn {
namespace sprite {
    
template <typename T>
concept HasNotEqualOperator = requires(T a, T b) {
    { a != b } -> std::same_as<bool>;
};

    class Map;
    class PropertyType;
    
    struct LockGuard {
        std::lock_guard<std::mutex> obj;
        LockGuard(Map*);
        ~LockGuard();
    };
    
    template<typename T>
    class Property;
    
    class ConstReference {
    private:
        const PropertyType* _type;
        const Map* _container;
        
    public:
        ConstReference(const Map& container, const PropertyType& type)
        : _type(&type), _container(&container)//, _name(name)
        { }
        
        bool operator==(const PropertyType& other) const;
        bool operator==(const ConstReference& other) const { return operator==(other.get()); }
        
        bool operator!=(const PropertyType& other) const;
        bool operator!=(const ConstReference& other) const { return operator!=(other.get()); }
        
        template<typename T>
        const Property<T>& toProperty() const;
        
        template<typename T>
        operator Property<T>&() const {
            return toProperty<T>();
        }
        
        template<typename T>
        operator const T() const;
        
        template<typename T>
        const T& value() const {
            return toProperty<T>().value();
        }
        
        template<typename T>
        bool is_type() const;
        
        const PropertyType& get() const { return *_type; }
        const Map& container() const { return *_container; }
        
        std::string toStr() const;
    };
    
    class Reference {
    private:
        PropertyType* _type;
        Map* _container;
        GETTER(std::string_view, name)
        
        
    public:
        template <typename T,
            std::enable_if_t<
                std::is_same_v<T, const char*> ||
                std::is_lvalue_reference_v<T&&>, int> = 0>
        Reference(Map& container, PropertyType& type, T&& name)
            : _type(&type), _container(&container), _name(name) { }

        // Delete the constructor for r-value std::string
        Reference(Map&, PropertyType&, std::string&&) = delete;
        
        bool operator==(const PropertyType& other) const;
        bool operator==(const Reference& other) const { return operator==(other.get()); }
        
        bool operator!=(const PropertyType& other) const;
        bool operator!=(const Reference& other) const { return operator!=(other.get()); }
        
        template<typename T>
        Property<T>& toProperty() const;
        
        template<typename T>
        operator Property<T>&() const {
            return toProperty<T>();
        }
        
        template<typename T>
        operator const T();
        
        template<typename T>
        T& value() const {
            return toProperty<T>().value();
        }

		template<typename T>
		void value(const T& v) {
			operator=(v);
		}
        
        template<typename T>
        bool is_type() const;
        
        PropertyType& get() const { return *_type; }
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
        std::unordered_map<std::string, Store, MultiStringHash, MultiStringEqual> _props;
        ska::bytell_hash_map<std::string, bool> _print_key;

        std::atomic<std::size_t> _id_counter{0u};
        std::unordered_map<std::size_t, std::function<void(Map*)>> _shutdown_callbacks;
        
        mutable std::mutex _mutex;
        GETTER_SETTER_I(bool, do_print, true)
        
    public:
        auto& mutex() const { return _mutex; }
        
        void dont_print(const std::string& name) {
            _print_key[name] = false;
        }
        
        void do_print(const std::string& name) {
            auto it = _print_key.find(name);
            if(it != _print_key.end())
                _print_key.erase(it);
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
        Map();
        Map(const Map& other);
        Map(Map&& other);
        Map& operator=(const Map& other);
        Map& operator=(Map&& other);
        
        ~Map();
        
        bool empty() const { std::unique_lock guard(mutex()); return _props.empty(); }
        size_t size() const { std::unique_lock guard(mutex()); return _props.size(); }
        
        bool operator==(const Map& other) const {
            if(other.size() != size())
                return false;
            
            std::scoped_lock guard(mutex(), other.mutex());
            auto it0 = other._props.begin();
            auto it1 = _props.begin();
            
            for(; it0 != other._props.end(); ++it0, ++it1) {
                if(!(it0->first == it1->first) || !(*it0->second == *it1->second)) {
                    print(it0->first," != ",it1->first," || ");
                    return false;
                }
            }
            
            return true;
        }
        
        template<typename Callback, typename Str>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
                    && std::convertible_to<Str, std::string>
        CallbackCollection register_callbacks(const std::initializer_list<Str>& names, Callback callback) {
            if constexpr(std::same_as<Str, const char*>) {
                std::vector<std::string_view> converted_names;
                for(const char* str : names) {
                    converted_names.emplace_back(str);
                }
                return register_callbacks_impl(converted_names, callback);
            } else
                return register_callbacks_impl(names, callback);
        }
        
        template<typename Callback, typename Str>
            requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
                    && std::convertible_to<Str, std::string>
        CallbackCollection register_callbacks(const std::vector<Str>& names, Callback callback) {
            return register_callbacks_impl(names, callback);
        }
        
        template<std::ranges::range Container, typename Callback>
        requires std::same_as<std::invoke_result_t<Callback, const char*>, void>
        CallbackCollection register_callbacks_impl(const Container& names, Callback callback) {
            CallbackCollection collection;
            for(const auto& name : names) {
                if(has(name)) {
                    collection._ids[std::string(name)] = operator[](name).get().registerCallback(callback);
                }
            }
            
            trigger_callbacks(collection);
            return collection;
        }
        
        void unregister_callbacks(CallbackCollection&& collection) {
            for(auto &[name, id] : collection._ids) {
                if(has(name)) {
                    operator[](name).get().unregisterCallback(id);
                }
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
                    || std::is_array_v<std::remove_reference_t<T>>
        Reference operator[](T&& name) {
            std::unique_lock guard(mutex());
            decltype(_props)::const_iterator it;

            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*>) {
                it = _props.find(std::string_view(name));
            } else if constexpr(std::is_array_v<std::remove_reference_t<T>>) {
                it = _props.find(std::string_view(name, std::size(name) - 1));  // -1 to remove null terminator
            } else {
                it = _props.find(name);
            }

            if(it != _props.end())
                return Reference(*this, *it->second, it->first);

            return Reference(*this, Property<bool>::InvalidProp, name);
        }
        
        template<typename T>
            requires std::is_same_v<T, const char*>
                    || std::is_lvalue_reference_v<T&&>
                    || std::is_array_v<std::remove_reference_t<T>>
        ConstReference operator[](T&& name) const {
            std::unique_lock guard(mutex());
            decltype(_props)::const_iterator it;

            if constexpr(std::is_same_v<std::remove_reference_t<T>, const char*>) {
                it = _props.find(std::string_view(name));
            } else if constexpr(std::is_array_v<std::remove_reference_t<T>>) {
                it = _props.find(std::string_view(name, std::size(name) - 1));  // -1 to remove null terminator
            } else {
                it = _props.find(name);
            }

            if(it != _props.end())
                return ConstReference(*this, *it->second);

            return ConstReference(*this, Property<bool>::InvalidProp);
        }
        
        template<typename T>
        Property<T>& get(const std::string_view& name) {
            std::unique_lock guard(mutex());
            auto it = _props.find(name);
            if(it != _props.end()) {
				return (Property<T>&)it->second->toProperty<T>();
            }
            
            return Property<T>::InvalidProp;
        }
        
        template<typename T>
        Property<T>& get(const std::string_view& name) const {
            std::unique_lock guard(mutex());
            auto it = _props.find(name);
            if(it != _props.end()) {
                return (Property<T>&)it->second->toProperty<T>();
            }
            
            return Property<T>::InvalidProp;
        }
        
        bool has(const std::string_view& name) const {
            std::unique_lock guard(mutex());
            return _props.contains(name);
        }
        
        template<typename T>
        bool is_type(const std::string_view& name, const T&) const {
            return get<T>(name).valid();
        }
        
        template<typename T>
        bool is_type(const std::string_view& name) const {
            return get<T>(name).valid();
        }
        
        bool has(const PropertyType& prop) const {
            return has(prop.name());
        }
        
        template<typename T>
        Property<T>& set(const std::string_view& name, const T& value) {
            operator[](name) = value;
        }
        
        template<typename T>
        void operator<<(const std::pair<std::string, const T> pair) {
            Property<T>& type = get(pair.first);
            
            if(type) {
                type = pair.second;
                
            } else {
                insert(Property<T>(this, pair.first, pair.second));
            }
        }
        
        template<typename T>
        Property<T>& insert(const std::string_view& name, const T& value) {
            return insert(Property<T>(name, value));
        }
        
        template<typename T>
        Property<T>& insert(const Property<T>& property) {
            Property<T> *property_;
            
            {
                if(has(property)) {
                    std::string e = "Property already "+((const PropertyType&)property).toStr()+" already exists.";
                    FormatError(e.c_str());
                    throw PropertyException(e);
                }
                
                property_ = new Property<T>(property.name(), property.value());
                {
                    auto ptr = Store(property_);
                    
                    std::unique_lock guard(mutex());
                    _props[ptr->name()] = ptr;
                }
            }
            
            return *property_;
        }
        
        void erase(const std::string& key) {
            if(not has(key)) {
                std::string e = "Map does not have key '"+key+"'.";
                FormatError(e.c_str());
                throw PropertyException(e);
            }
            
            std::unique_lock guard(mutex());
            _props.erase(key);
        }
        
        std::vector<std::string> keys() const {
            std::unique_lock guard(mutex());
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
        return _type->toProperty<T>();
    }
    
    template<typename T>
    Reference::operator const T() {
        LockGuard guard(_container);
        Property<T> *tmp = dynamic_cast<Property<T>*>(_type);
        if (tmp) {
            return tmp->value();
        }
        
        std::string e = _type->valid()
            ? "Cannot find variable '" + (std::string)_name + "' of type " + Meta::name<T>() + " in map."
            : "Cannot cast '" + (std::string)_name + "' of type " + (_type->valid() ? _type->type_name() : "<variable not found>") + " to "+ Meta::name<T>() +".";
        FormatError(e.c_str());
        throw PropertyException(e);
    }
    
    template<typename T>
    bool Reference::is_type() const {
        return _type->toProperty<T>().valid();
    }
    
template<typename T>
    requires (not std::convertible_to<T, const char*>)
void Reference::operator=(const T& value) {
    if (_type->valid()) {
        _type->operator=(value);
        
    }
    else {
        _container->insert(_name, value);
    }
}

template<typename T>
    requires (std::convertible_to<T, const char*>)
void Reference::operator=(const T& value) {
    if (_type->valid()) {
        _type->operator=(std::string(value));
        
    }
    else {
        _container->insert(_name, std::string(value));
    }
}

    template<typename T>
    const Property<T>& ConstReference::toProperty() const {
        return _type->toProperty<T>();
    }
    
    template<typename T>
    ConstReference::operator const T() const {
        Property<T> *tmp = dynamic_cast<Property<T>*>(_type);
        if (tmp) {
            return tmp->value();
        }
        
        std::string e = "Cannot cast " + _type->toStr() + " to value type "+ Meta::name<T>() +" .";
        FormatError(e.c_str());
        throw PropertyException(e);
    }
    
    template<typename T>
    bool ConstReference::is_type() const {
        return _type->toProperty<T>().valid();
    }

    template<typename T>
    void PropertyType::operator=(const T& value) {
        auto ptr = static_cast<Property<T>*>(this);
        if(ptr->valid()) {
            *ptr = value;
            
        } else {
            std::stringstream ss;
            ss << "Reference to "+toStr()+" cannot be cast to type of ";
            ss << Meta::name<T>();
            FormatError(ss.str().c_str());
            throw PropertyException(ss.str());
        }
    }

    template<typename T>
    void Property<T>::value(const T& v) {
        if constexpr(HasNotEqualOperator<T>) {
            if(std::unique_lock guard(_property_mutex);
               v != _value)
            {
                _value = v;
                
                guard.unlock();
                triggerCallbacks();
            }
            return;
            
        } else {
            {
                std::unique_lock guard(_property_mutex);
                _value = v;
            }
            
            triggerCallbacks();
        }
    }



    template<typename T>
    void Property<T>::copy_to(Map* other) const {
        if (other->has(_name))
            other->erase(_name);

        std::unique_lock guard(_property_mutex);
        other->insert(_name, _value);
    }
}
}

