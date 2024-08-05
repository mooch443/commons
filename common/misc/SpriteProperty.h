#pragma once

#include <commons.pc.h>
#include <file/Path.h>
#include <file/PathArray.h>
#include <misc/CallbackManager.h>

namespace cmn {
    namespace sprite {
        
        class PropertyException : public virtual UtilsException {
        public:
            PropertyException(const std::string& str) : UtilsException(str) { }
            ~PropertyException() throw() {}
        };
        
        template<typename ValueType = double>
        class Property;
        class Map;
        
        /**
         * @class PropertyType
         * Represents a generic property type that serves as the base for specialized property types.
         * Provides essential functionalities such as serialization, comparison, and string conversion.
         */
        class PropertyType {
        protected:
            const std::string _name; ///< Name of the property.
            const std::string _type_name; ///< Name of the type of the property
            
            // Mutex to ensure thread safety for concurrent access.
            // NOTE: Currently, the mutex is defined but not used in any member functions.
            // Ensure you lock this mutex in member functions that modify the object's state if they can be called concurrently.
            mutable std::shared_mutex _property_mutex;

            // The following are getters for specific flags.
            GETTER(bool, is_array); ///< Indicates if the property is of array type.
            GETTER(bool, is_enum); ///< Indicates if the property is of enum type.
            
            // Lambda functions to allow custom behaviors. These can be overridden as needed.
            std::function<void(const std::string&)> _set_value_from_string; ///< Setter function from string.

            // More lambda functions for enum type properties.
            GETTER(std::function<std::vector<std::string>()>, enum_values);///< Getter for the enum values.
            GETTER(std::function<uint32_t()>, enum_index);///< Getter for the index of an enum value.
            
            CallbackManager _callbacks; ///< Manages callbacks associated with this property.
            GETTER_SETTER_I(bool, do_print, false);
            
            friend class sprite::Map;
            
        public:
            // Constructors
            PropertyType(const std::string& type_name) : PropertyType(type_name, "<invalid>") {}

            /**
             * Primary constructor for the PropertyType.
             * @param map Pointer to associated map object.
             * @param name Name of the property.
             */
            PropertyType(const std::string& type_name, const std::string_view& name)
                : _name(name), _type_name(type_name)
            {
                // Set default behaviors for lambda functions.
                _set_value_from_string = [this](const std::string&){
                    throw U_EXCEPTION("Uninitialized function set_value_from_string (", _name, ").");
                };
                _enum_values = []() -> std::vector<std::string> {
                    throw U_EXCEPTION("PropertyType::enum_values() not initialized.");
                };
                _enum_index = []() -> uint32_t {
                    throw U_EXCEPTION("PropertyType::enum_index() not initialized");
                };
            }
            
            virtual ~PropertyType() = default; ///< Default virtual destructor.
            
            virtual void copy_to(Map* other) const = 0; ///< Copy function to duplicate this property to another map.
            
            /**
             * Registers a new callback function and returns its unique ID.
             * The callback will be invoked when certain events or changes occur.
             *
             * @param callback The callback function to register.
             * @return A unique identifier for the registered callback.
             */
            std::size_t registerCallback(const std::function<void(std::string_view)>& callback) {
                return _callbacks.registerCallback(callback);
            }

            /**
             * Unregisters (removes) a previously registered callback using its unique ID.
             *
             * @param id The unique identifier of the callback to unregister.
             */
            void unregisterCallback(std::size_t id) {
                _callbacks.unregisterCallback(id);
            }
            
            void triggerCallbacks() {
                _callbacks.callAll(_name);
            }
            
            void triggerCallback(std::size_t id) {
                _callbacks.call(id, _name);
            }
            
            /**
             * Sets the value of the property from a string-serialized object.
             * @param str String representation of the value.
             */
            void set_value_from_string(const std::string& str) {
                try {
                    _set_value_from_string(str);
                } catch(const std::invalid_argument& e) {
                    throw U_EXCEPTION("Cannot set ", this->name().c_str()," to (",utils::ShortenText(str.c_str(), 100),"): ", std::string(e.what()));
                }
            }
            
            /**
             * Compares the current property with another for equality.
             * Must be overridden by derived classes.
             * @param other The other property to compare with.
             * @return True if equal, false otherwise.
             */
            virtual bool operator==(const PropertyType& other) const = 0;
            
            /**
             * Compares the current property with another for inequality.
             * @param other The other property to compare with.
             * @return True if not equal, false otherwise.
             */
            bool operator!=(const PropertyType& other) const {
                return !operator==(other);
            }
            
            /**
             * Assigns a value to the property.
             * @param value The value to assign.
             */
            template<typename T>
            void operator=(const T& value);
            
            /**
             * Retrieves the name of the property.
             * @return The property's name.
             */
            const std::string& name() const { return _name; }

            /**
             * Checks the validity state of the property.
             * @return True if valid, false otherwise.
             */
            virtual bool valid() const = 0;
            
            /**
             * Serializes the property to a JSON object.
             * @return A JSON representation of the property.
             */
            virtual nlohmann::json to_json() const = 0;

            template<typename T>
            Property<T>& toProperty() {
                Property<T> *tmp = dynamic_cast<Property<T>*>(this);
                if(not tmp) {
                    throw U_EXCEPTION("Cannot cast ", type_name(), " to ", cmn::type_name<T>(), " in ", *this);
                }
                return *tmp;
            }

            template<typename T>
            const Property<T>& toProperty(cmn::source_location loc = cmn::source_location::current()) const {
                const Property<T> *tmp = dynamic_cast<const Property<T>*>(this);
                if(not tmp)
                    throw PropertyException("Cannot cast " + toStr() + " to const reference type ("+Meta::name<T>()+ ") called at: "+Meta::toStr(loc.file_name()) + ":"+Meta::toStr(loc.line()) + ".");
                return *tmp;
            }
            
            /*template<typename T>
            operator Property<T>&() {
                return toProperty<T>();
            }
            
            template<typename T>
            operator const Property<T>&() const {
                return toProperty<T>();
            }

            template<typename T>
            operator const T&() const {
                return value<T>();
            }*/
            
            template<typename T>
            auto value(cmn::source_location loc = cmn::source_location::current()) const {
                return toProperty<T>(loc).value();
            }
            
            /**
             * Converts the property to a specific type.
             * @return The property as the specified type.
             */
            /*template<typename T>
            operator typename std::remove_const<T>::type () {
                return toProperty<T>().value();
            }*/
            
            /**
             * Retrieves the type name of the property.
             * @return The type name.
             */
            virtual std::string_view type_name() const {
                return _type_name;
            }
            
            /**
             * Converts the property value to its string representation.
             * @return The string representation of the value.
             */
            virtual std::string valueString() const {
                if(!valid())
                    throw PropertyException("ValueString of invalid PropertyType.");
                throw PropertyException("Cannot use valueString of PropertyType directly.");
            }
            
            /**
             * Generates a string representation of the property.
             * @return A string representing the property.
             */
            std::string toStr() const {
                return "Property<"+(std::string)type_name()+">"
                     + "('" + _name + "'"
                     + ") = "+valueString();
            }
            
            template<typename T>
            bool is_type() const {
                return _type_name == cmn::type_name<T>();
            }
        };
        
        class Reference;
        
        namespace detail {
            struct No {};
            template<typename T, typename Arg> No operator== (const T&, const Arg&);
        }
        
        template<typename T, typename Arg = T>
            requires (not is_instantiation<std::function, T>::value)
        struct has_equals
        {
            constexpr static const bool value = !std::is_same<decltype(*(T*)(0) == *(Arg*)(0)), detail::No>::value;
        };

        template<class ValueType>
        class Property : public PropertyType {
        protected:
            static constexpr bool trivial = std::is_trivially_copyable_v<ValueType> 
                && std::is_move_constructible_v<ValueType>
                && std::is_move_assignable_v<ValueType>
                && std::is_copy_constructible_v<ValueType>
                && std::is_copy_assignable_v<ValueType>;
            using BaseStoreType = std::optional<ValueType>;
            using StoreType = std::conditional_t<
                trivial,
                std::atomic<std::optional<ValueType>>,
                std::optional<ValueType>
            >;
            StoreType _value;
            
        public:
            Property() : PropertyType(Meta::name<ValueType>())
            {
                init();
            }
            
            Property(const std::string_view& name, const ValueType& v = ValueType())
                : PropertyType(Meta::name<ValueType>(), name), _value(v)
            {
                init();
            }
            
            void copy_to(Map* other) const override;
            
            void init() {
                _set_value_from_string = [this](const std::string& str) {
                    *this = Meta::fromStr<ValueType>(str);
                };
                
                _is_enum = cmn::is_enum<ValueType>::value;
                _is_array = is_container<ValueType>::value;
                
                init_enum();
            }
            
            template<typename T = ValueType>
            void init_enum(typename std::enable_if_t<cmn::is_enum<T>::value, T> * = nullptr) {
                auto options = ValueType::Data::str();
                auto values = std::vector<std::string>(options.begin(), options.end());
                _enum_values = [values]() { return values; };
                _enum_index = [this]() -> uint32_t {
                    return (uint32_t)this->value();
                };
            }
            
            template<typename T = ValueType>
            void init_enum(typename std::enable_if_t<!cmn::is_enum<T>::value, T> * = nullptr) {
                _enum_values = []() -> std::vector<std::string> { throw U_EXCEPTION("This type is not an Enum, so enum_values() cannot be called."); };
            }
            
            template<typename K>
                requires (not is_instantiation<std::function, K>::value)
            bool equals(const K& other, const typename std::enable_if< !std::is_same<cv::Mat, K>::value && has_equals<K>::value, K >::type* = NULL) const {
                if constexpr(trivial) {
                    auto v = _value.load();
                    return v.has_value() && other == v.value();
                } else {
                    std::shared_lock guard(_property_mutex);
                    return _value.has_value() && other == _value.value();
                }
            }
            
            template<typename K>
                requires (not is_instantiation<std::function, K>::value)
            bool equals(const K& other, const typename std::enable_if< std::is_same<cv::Mat, K>::value, K >::type* = NULL) const {
                std::shared_lock guard(_property_mutex);
                if(not _value.has_value())
                    return false;
                return cv::countNonZero(_value.value() != other) == 0;
            }
            
            template<typename K>
                requires (is_instantiation<std::function, K>::value)
            bool equals(const K&, const typename std::enable_if< is_instantiation<std::function, K>::value, K >::type* = NULL) const {
                return false;
            }
            
            bool operator==(const PropertyType& other) const override {
                if(not valid() && not other.valid())
                    return true;
                
                if(other.type_name() != type_name())
                    return false;
                
				const Property& other_ = (const Property<ValueType>&) other;
                return other_.if_value([this](const ValueType& value)
                    -> bool
                {
                    return this->equals(value);
                }, []() -> bool{
                    return false;
                });
            }
            
            void operator=(const Reference& other);

            void operator=(const PropertyType& other);
            
        public:
            void operator=(const ValueType& v) { value(v); }
            void value(const ValueType& v);
            
            bool valid() const override {
                if constexpr(trivial)
                    return _value.load().has_value();
                else {
                    std::shared_lock guard(_property_mutex);
                    return _value.has_value();
                }
            }
            
            nlohmann::json to_json() const override {
                if constexpr(trivial) {
                    auto v = _value.load();
                    if(not v.has_value())
                        throw PropertyException("No value stored in "+name()+".");
                    return cvt2json(v.value());
                } else {
                    std::shared_lock guard(_property_mutex);
                    if(not _value.has_value())
                        throw PropertyException("No value stored in "+name()+".");
                    return cvt2json(_value.value());
                }
            }
            
            template<typename K = ValueType>
                requires (HasNotEqualOperator<K> && trivial)
            bool assign_if(const ValueType& next) {
                BaseStoreType v = _value.load();
                if(v.has_value()) {
                    if(v.value() != next) {
                        //return _value.compare_exchange_strong(v, BaseStoreType(next));
                        _value.store(next);
                        return true;
                    }
                    
                } else {
                    //return _value.compare_exchange_strong(v, BaseStoreType(next));
                    _value.store(next);
                    return true;
                }
                
                return false;
            }
            
            template<typename K = ValueType>
                requires (HasNotEqualOperator<K> && not trivial)
            bool assign_if(const ValueType& next) {
                std::unique_lock guard(_property_mutex);
                if(_value.has_value()) {
                    if(_value.value() != next) {
                        _value = next;
                        return true;
                    }
                    
                } else {
                    _value = next;
                    return true;
                }
                
                return false;
            }

            template<typename K = ValueType>
                requires (not HasNotEqualOperator<K>)
            bool assign_if(const ValueType& next) {
                if constexpr(trivial) {
                    _value.store(BaseStoreType(next));
                } else {
                    std::unique_lock guard(_property_mutex);
                    _value = next;
                }
                
                return true;
            }
            
            template<typename If_t,
                     typename Else_t,
                     typename R = typename ::cmn::detail::return_type<If_t>::type>
                requires requires(If_t _if, Else_t _e, ValueType t) {
                    { _if(t) } -> std::same_as<R>;
                    { _e() } -> std::same_as<R>;
                }
            R if_value(If_t&& If, Else_t&& Else) const {
                if constexpr(trivial) {
                    auto v = _value.load();
                    if(v.has_value())
                        return If(v.value());
                    else
                        return Else();
                    
                } else {
                    std::shared_lock guard(_property_mutex);
                    if(_value.has_value()) {
                        return If(_value.value());
                    } else {
                        return Else();
                    }
                }
            }
            
            //operator const ValueType() const { return value(); }
            
            ValueType value() const {
                if constexpr(trivial) {
                    return _value.load().value();
                } else {
                    std::shared_lock guard(_property_mutex);
                    //if constexpr(std::is_trivially_copyable_v<ValueType>)
                    return _value.value();
                    //else
                    //    return
                    //TransparentLock<ValueType>(_value.value(), std::move(guard));
                }
            }
            //ValueType& value() { return _value.value(); }
            std::string valueString() const override {
                return Meta::toStr<ValueType>(value());
            }
            
            std::string toStr() const {
                if(!valid())
                    return PropertyType::toStr();
                    
                std::stringstream ss;
                ss << PropertyType::toStr();
                auto str = Meta::toStr<ValueType>(value());
                if(str.length() > 1000)
                    str = str.substr(0, 1000) + " (shortened)...";
                ss << " = " << str;
                return ss.str();
            }
        };
    
    Property(const char*) -> Property<std::string>;
    }
}
            
#include "SpriteMap.h"

namespace cmn {
    namespace sprite {
        template<typename T>
        void Property<T>::operator=(const Reference& other) {
            const Property& _other = other.toProperty<T>();
            if (_other)
                *this = _other.value();
            else
                throw PropertyException("Cannot assign " + other.toStr() + " to " + this->toStr());
        }
        
        template<typename T>
        void Property<T>::operator=(const PropertyType& other) {
            const Property& _other = other.operator const cmn::sprite::Property<T>&();
            if(_other.valid()) {
                *this = _other.value();
                
            } else {
                throw PropertyException("Cannot assign "+other.toStr()+" to "+this->toStr());
            }
        }
    }
}

