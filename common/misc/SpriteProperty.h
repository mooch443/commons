#pragma once

#include <misc/defines.h>
#include <misc/metastring.h>
#include <misc/vec2.h>
#include <nlohmann/json.hpp>
#include <file/Path.h>
#include <file/PathArray.h>
#include <misc/CallbackManager.h>

namespace cmn {
    namespace sprite {
        
        template<typename VT>
        auto cvt2json(const VT & v) -> nlohmann::json {
            if constexpr (is_numeric<VT>)
                return nlohmann::json(v);
            if constexpr (_clean_same<VT, bool>)
                return nlohmann::json((bool)v);
            if constexpr (_clean_same<VT, file::PathArray>) {
                auto i = v.source();
                return nlohmann::json(i);
            }
            if constexpr (_clean_same<VT, file::Path>) {
                auto i = v.str();
                return nlohmann::json(i.c_str());
            }
            if constexpr (is_map<VT>::value) {
                auto a = nlohmann::json::object();
                for (auto& [key, i] : v) {
                    if constexpr(std::is_same_v<decltype(key), std::string>)
                        a[key] = cvt2json(i);
                    else if constexpr(std::is_convertible_v<decltype(key), std::string>)
                        a[std::string(key)] = cvt2json(i);
                    else
                        a[Meta::toStr(key)] = cvt2json(i);
				}
                return a;
            }
            if constexpr (is_container<VT>::value) {
                auto a = nlohmann::json::array();
                for (const auto& i : v) {
                    a.push_back(cvt2json(i));
                }
                return a;
            }
            if constexpr (std::is_same_v<VT, std::string>) {
                std::string str = v;
				return nlohmann::json(v.c_str());
			}
            if constexpr (std::is_convertible_v<VT, std::string>) {
                std::string str = v;
                return nlohmann::json(v.c_str());
            }
            nlohmann::json json = Meta::toStr(v);
            return json;
        }

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
            bool _valid = false; ///< Flag indicating if the property is valid.
            
            // Mutex to ensure thread safety for concurrent access.
            // NOTE: Currently, the mutex is defined but not used in any member functions.
            // Ensure you lock this mutex in member functions that modify the object's state if they can be called concurrently.
            mutable std::mutex _property_mutex;

            // The following are getters for specific flags.
            GETTER(bool, is_array) ///< Indicates if the property is of array type.
            GETTER(bool, is_enum) ///< Indicates if the property is of enum type.
            
            // Lambda functions to allow custom behaviors. These can be overridden as needed.
            std::function<void(const std::string&)> _set_value_from_string; ///< Setter function from string.
            std::function<std::string()> _type_name = [](){return "unknown";}; ///< Getter for the type name.
            std::function<nlohmann::json()> _to_json = [](){return nlohmann::json();}; ///< Function to serialize property to JSON.

            // More lambda functions for enum type properties.
            GETTER(std::function<std::vector<std::string>()>, enum_values) ///< Getter for the enum values.
            GETTER(std::function<size_t()>, enum_index) ///< Getter for the index of an enum value.
            
            CallbackManager _callbacks; ///< Manages callbacks associated with this property.
            
        public:
            // Constructors
            PropertyType() : PropertyType("<invalid>") {
                _valid = false;
            }

            /**
             * Primary constructor for the PropertyType.
             * @param map Pointer to associated map object.
             * @param name Name of the property.
             */
            PropertyType(const std::string_view& name)
                : _name(name), _valid(true)
            {
                // Set default behaviors for lambda functions.
                _set_value_from_string = [this](const std::string&){
                    throw U_EXCEPTION("Uninitialized function set_value_from_string (", _name, ").");
                };
                _enum_values = []() -> std::vector<std::string> {
                    throw U_EXCEPTION("PropertyType::enum_values() not initialized.");
                };
                _enum_index = []() -> size_t {
                    throw U_EXCEPTION("PropertyType::enum_index() not initialized");
                };
            }
            
            virtual ~PropertyType() = default; ///< Default virtual destructor.
            
            virtual void copy_to(Map* other) const; ///< Copy function to duplicate this property to another map.
            
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
                    throw U_EXCEPTION("Cannot set ", this->name().c_str()," to (",str,"): ", std::string(e.what()));
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
             * Sets the validity state of the property.
             * @param valid The validity state to set.
             */
            void valid(bool valid) { _valid = valid; }

            /**
             * Checks the validity state of the property.
             * @return True if valid, false otherwise.
             */
            bool valid() const { return _valid; }
            
            /**
             * Serializes the property to a JSON object.
             * @return A JSON representation of the property.
             */
            auto to_json() const {
				return _to_json();
			}

            template<typename T>
            Property<T>& toProperty() {
                Property<T> *tmp = dynamic_cast<Property<T>*>(this);
                return tmp ? *tmp : Property<T>::InvalidProp;
            }

            template<typename T>
            const Property<T>& toProperty() const {
                const Property<T> *tmp = dynamic_cast<const Property<T>*>(this);
                return tmp ? *tmp : Property<T>::InvalidProp;
            }
            
            template<typename T>
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
            }
            
            template<typename T>
            const T& value(cmn::source_location loc = cmn::source_location::current()) const {
                const Property<T>& p = toProperty<T>();
                if (p.valid())
                    return p.value();
                
                throw PropertyException("Cannot cast " + toStr() + " to const reference type ("+Meta::name<T>()+ ") called at: "+Meta::toStr(loc.file_name()) + ":"+Meta::toStr(loc.line()) + ".");
            }
            
            /**
             * Converts the property to a specific type.
             * @return The property as the specified type.
             */
            template<typename T>
            operator typename std::remove_const<T>::type &() {
                Property<T>& p = toProperty<T>();
                if (p.valid())
                    return p.value();

                throw PropertyException("Cannot cast " + toStr() + " to reference type ("+ Meta::name<T>()+").");
            }
            
            /**
             * Retrieves the type name of the property.
             * @return The type name.
             */
            virtual const std::string type_name() const {
                return _type_name();
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
                return "Property<"+type_name()+">"
                     + "('" + _name + "'"
                     + ")";
            }
        };
        
        class Reference;
        
        namespace detail {
            struct No {};
            template<typename T, typename Arg> No operator== (const T&, const Arg&);
        }
        
        template<typename T, typename Arg = T>
        struct has_equals
        {
            constexpr static const bool value = !std::is_same<decltype(*(T*)(0) == *(Arg*)(0)), detail::No>::value;
        };

        template<class ValueType>
        class Property : public PropertyType {
        public:
            static Property<ValueType> InvalidProp;
            
        protected:
            ValueType _value;
            
        public:
            Property() 
                : PropertyType(), _value(ValueType())
            {
                init();
            }
            
            Property(const std::string_view& name, const ValueType& v = ValueType())
                : PropertyType(name), _value(v)
            {
                init();
            }
            
            void copy_to(Map* other) const override;
            
            void init() {
                _set_value_from_string = [this](const std::string& str) {
                    *this = Meta::fromStr<ValueType>(str);
                };
                
                _type_name = [](){ return Meta::name<ValueType>(); };
                _to_json = [this]() {
                    return cvt2json(_value);
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
                _enum_index = [this]() -> size_t { return (size_t)this->value().value(); };
            }
            
            template<typename T = ValueType>
            void init_enum(typename std::enable_if_t<!cmn::is_enum<T>::value, T> * = nullptr) {
                _enum_values = []() -> std::vector<std::string> { throw U_EXCEPTION("This type is not an Enum, so enum_values() cannot be called."); };
            }
            
            template<typename K>
            bool equals(const K& other, const typename std::enable_if< !std::is_same<cv::Mat, K>::value && has_equals<K>::value, K >::type* = NULL) const {
                return other == _value;
            }
            
            template<typename K>
            bool equals(const K& other, const typename std::enable_if< std::is_same<cv::Mat, K>::value, K >::type* = NULL) const {
                return cv::countNonZero(_value != other) == 0;
            }
            
            /*template<typename K>
            bool equals(const K& other, const typename std::enable_if< ! has_equals<K>::value, K >::type* = NULL) const {
                return &other == &_value;
            }*/
            
            bool operator==(const PropertyType& other) const override {
				const Property& other_ = (const Property<ValueType>&) other;
                return other_.valid() && equals(other_._value);
            }
            
            void operator=(const Reference& other);

            void operator=(const PropertyType& other);
            
        public:
            void operator=(const ValueType& v) { value(v); }
            void value(const ValueType& v);
            
            operator const ValueType() const { return value(); }
            const ValueType& value() const { return _value; }
            ValueType& value() { return _value; }
            std::string valueString() const override { return Meta::toStr<ValueType>(value()); }
            
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

    template<typename T>
    Property<T> Property<T>::InvalidProp = Property<T>();
    }
}
            
#include "SpriteMap.h"

namespace cmn {
    namespace sprite {
        template<typename T>
        void Property<T>::operator=(const Reference& other) {
            std::unique_lock guard(_property_mutex);
            const Property& _other = other.toProperty<T>();
            if (_other)
                *this = _other;
            else
                throw PropertyException("Cannot assign " + other.toStr() + " to " + this->toStr());
        }
        
        template<typename T>
        void Property<T>::operator=(const PropertyType& other) {
            std::unique_lock guard(_property_mutex);
            const Property& _other = other.operator const cmn::sprite::Property<T>&();
            if(_other.valid()) {
                *this = _other.value();
                
            } else {
                throw PropertyException("Cannot assign "+other.toStr()+" to "+this->toStr());
            }
        }
    }
}

