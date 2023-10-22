#pragma once

#include <misc/defines.h>
#include <misc/metastring.h>
#include <misc/vec2.h>
#include <nlohmann/json.hpp>
#include <file/Path.h>
#include <file/PathArray.h>

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
        
        class PropertyType {
        protected:
            std::string _name;
            bool _valid;
            std::mutex _property_mutex;
            
            GETTER(bool, is_array)
            GETTER(bool, is_enum)
            
            std::function<void(const std::string&)> _set_value_from_string;
            std::function<std::string()> _type_name;
            std::function<nlohmann::json()> _to_json;
            GETTER(std::function<std::vector<std::string>()>, enum_values)
            GETTER(std::function<size_t()>, enum_index)
            
            GETTER_SETTER_PTR_I(Map*, map, nullptr)
            
        public:
            PropertyType(Map *map)
                : _name("<invalid>"), _valid(false), _is_array(false), _is_enum(false),
                _set_value_from_string([this](const std::string&){throw U_EXCEPTION("Uninitialized function set_value_from_string (",_name,").");}),
                _type_name([](){return "unknown";}),
                _to_json([](){return nlohmann::json();}),
                _enum_values([]() -> std::vector<std::string> {     throw U_EXCEPTION("PropertyType::enum_values() not initialized."); }),
                _enum_index([]() -> size_t{ throw U_EXCEPTION("PropertyType::enum_index() not initialized"); }),
                _map(map)
            { }
            
            PropertyType(Map *map, const std::string_view& name)
                : _name(name), _valid(true), _is_array(false), _is_enum(false),
                _set_value_from_string([this](const std::string&){throw U_EXCEPTION("Uninitialized function set_value_from_string (",_name,").");}),
                _type_name([](){return "unknown";}),
                _enum_values([]() -> std::vector<std::string> {     throw U_EXCEPTION("PropertyType::enum_values() not initialized."); }),
                _enum_index([]() -> size_t{ throw U_EXCEPTION("PropertyType::enum_index() not initialized"); }),
                _map(map)
            { }
            
            virtual ~PropertyType() = default;
            
            virtual void copy_to(Map* other) const;
            
            void set_value_from_string(const std::string& str) {
                try {
                    _set_value_from_string(str);
                } catch(const std::invalid_argument& e) {
                    throw U_EXCEPTION("Cannot set ", this->name().c_str()," to (",str,"): ", std::string(e.what()));
                }
            }
            
            virtual bool operator==(const PropertyType& other) const = 0;
            bool operator!=(const PropertyType& other) const {
                return !operator==(other);
            }
            
            template<typename T>
            void operator=(const T& value);
            
            const std::string& name() const { return _name; }
            
            void valid(bool valid) { _valid = valid; }
            bool valid() const { return _valid; }

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

            template<typename T>
            operator typename std::remove_const<T>::type &() {
                Property<T>& p = toProperty<T>();
                if (p.valid())
                    return p.value();

                throw PropertyException("Cannot cast " + toStr() + " to reference type ("+ Meta::name<T>()+").");
            }
            
            virtual const std::string type_name() const {
                return _type_name();
            }
            
            virtual std::string valueString() const {
                if(!valid())
                    throw PropertyException("ValueString of invalid PropertyType.");
                throw PropertyException("Cannot use valueString of PropertyType directly.");
            }
            
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
            Property(Map *map)
            : PropertyType(map), _value(ValueType())
            {
                init();
            }
            
            Property(Map *map, const std::string_view& name, const ValueType& v = ValueType())
                : PropertyType(map, name), _value(v)
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
                
        template<typename T>
        Property<T> Property<T>::InvalidProp(NULL);
    
    Property(const char*) -> Property<std::string>;
    }
}
            
#include "SpriteMap.h"

namespace cmn {
    namespace sprite {
        template<typename T>
        void Property<T>::operator=(const Reference& other) {
            LockGuard guard(map());
            const Property& _other = other.toProperty<T>();
            if (_other)
                *this = _other;
            else
                throw PropertyException("Cannot assign " + other.toStr() + " to " + this->toStr());
        }
        
        template<typename T>
        void Property<T>::operator=(const PropertyType& other) {
            LockGuard guard(map());
            const Property& _other = other.operator const cmn::sprite::Property<T>&();
            if(_other.valid()) {
                *this = _other.value();
                
            } else {
                throw PropertyException("Cannot assign "+other.toStr()+" to "+this->toStr());
            }
        }
    }
}

