#include "SpriteMap.h"

namespace std {
/*size_t hash<cmn::sprite::PNameRef>::operator()(const cmn::sprite::PNameRef& k) const
{
    return std::hash<std::string>{}(k.name());
}*/
}

namespace cmn {
namespace sprite {

Map::Map() : _do_print(true) {
}

Map::Map(const Map& other) {
    _props = other._props;
    _print_key = other._print_key;
    _do_print = other._do_print;
}

Map::Map(Map&& other) 
    : _props(std::move(other._props)),
      _print_key(std::move(other._print_key)),
      _do_print(std::move(other._do_print))
{ }

Map& Map::operator=(const Map& other) {
	_props = other._props;
    _print_key = other._print_key;
	_do_print = other._do_print;
	return *this;
}

Map& Map::operator=(Map&& other) {
	_props = std::move(other._props);
	_print_key = std::move(other._print_key);
	_do_print = std::move(other._do_print);
	return *this;
}

Map::~Map() {
    decltype(_shutdown_callbacks) copy;
    {
        std::lock_guard guard(_mutex);
        _props.clear();
        _print_key.clear();
        copy = _shutdown_callbacks;
    }
    
    for(auto &[k,s] : copy) {
        s(this);
    }
}
    
    // --------- PNameRef
    
    LockGuard::LockGuard(Map*map) : obj(map->mutex()) {
        //obj = std::make_unique<decltype(obj)::element_type>(map->mutex());
    }
    LockGuard::~LockGuard() {
    }
    
    /*PNameRef::PNameRef(const std::string_view& name)
        : _ref(NULL)
    {}

    PNameRef::PNameRef(const std::string& name)
        : _ref(NULL)
    {
    }
    
    PNameRef::PNameRef(const std::shared_ptr<PropertyType>& ref)
        : _ref(ref)
    {}
    
    //PNameRef::PNameRef(const PropertyType& ref) : PNameRef(&ref) { }
    
    const std::string& PNameRef::name() const {
        static std::string null;
        return _ref 
            ? _ref->name()
            : null;
    }
    
    bool PNameRef::operator==(const PNameRef& other) const {
        return (_ref && _ref == other._ref)
        || (*this == other.name());
    }
    
    bool PNameRef::operator==(const std::string& other) const {
        return name() == other;
    }
    
    bool PNameRef::operator<(const PNameRef& other) const {
        return name() < other.name();
    }*/
    
    
    // -------- REFERENCE
    
    //Reference::Reference(Map& container, PropertyType& type)
    //: Reference(container, type, type.name()) {}

    std::string Reference::toStr() const {
        return _type->toStr();
    }
    
    /*_PRINT_NAME_RETURN_TYPE Reference:: _PRINT_NAME_HEAD {
        return _type.print_name();
    }*/
    
    bool Reference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool Reference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }
    
    std::string ConstReference::toStr() const {
        return _type->toStr();
    }
    
    /*_PRINT_NAME_RETURN_TYPE ConstReference:: _PRINT_NAME_HEAD {
        return _type.print_name();
    }*/
    
    bool ConstReference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool ConstReference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }

}
}
