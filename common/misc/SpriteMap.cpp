#include "SpriteMap.h"

namespace std {
/*size_t hash<cmn::sprite::PNameRef>::operator()(const cmn::sprite::PNameRef& k) const
{
    return std::hash<std::string>{}(k.name());
}*/
}

namespace cmn {
namespace sprite {

Map::Map(const Map& other) {
    for(auto &[name, ptr] : other._props) {
        ptr->copy_to(this);
    }
    _print_by_default = other._print_by_default;
}

Map::Map(Map&& other) 
    : _props(std::move(other._props)),
    _id_counter(other._id_counter.load()),
    _shutdown_callbacks(std::move(other._shutdown_callbacks)),
    _print_by_default(std::move(other._print_by_default))
{ }

Map& Map::operator=(const Map& other) {
    _print_by_default = false;
    
    std::unordered_map<std::string, CallbackManager> callbacks;
    for(auto &&[key, value] : _props) {
        callbacks[key] = value->_callbacks;
    }
    _props.clear();
    
    for(auto &[name, ptr] : other._props) {
        ptr->copy_to(this);
    }
    
    for(auto &&[key, c] : callbacks) {
        if(_props.contains(key))
            _props[key]->_callbacks = c;
    }
    
    _print_by_default = other._print_by_default;
	return *this;
}

Map& Map::operator=(Map&& other) {
	_props = std::move(other._props);
    _print_by_default = std::move(other._print_by_default);
    _shutdown_callbacks = std::move(other._shutdown_callbacks);
    _id_counter = other._id_counter.load();
	return *this;
}

Map::~Map() {
    decltype(_shutdown_callbacks) copy;
    {
        auto guard = LOGGED_LOCK(_mutex);
        _props.clear();
        copy = _shutdown_callbacks;
    }
    
    for(auto &[k,s] : copy) {
        s(this);
    }
}
    
    // -------- REFERENCE
    
    std::string Reference::toStr() const {
        if(not _type)
            return "null";
        return _type->toStr();
    }
    
    bool Reference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool Reference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }

bool Reference::valid() const {
    return _type && _type->valid();
}

bool ConstReference::valid() const {
    return _type && _type->valid();
}

std::string_view Reference::type_name() const {
    if(not _type)
        throw InvalidArgumentException("Reference to an invalid option.");
    return _type->type_name();
}
std::string_view ConstReference::type_name() const {
    if(not _type)
        throw InvalidArgumentException("Reference to an invalid option.");
    return _type->type_name();
}
    
    std::string ConstReference::toStr() const {
        if(not _type)
            return "null";
        return _type->toStr();
    }
    
    bool ConstReference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool ConstReference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }

}
}
