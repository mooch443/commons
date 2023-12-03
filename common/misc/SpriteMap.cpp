#include "SpriteMap.h"

namespace std {
/*size_t hash<cmn::sprite::PNameRef>::operator()(const cmn::sprite::PNameRef& k) const
{
    return std::hash<std::string>{}(k.name());
}*/
}

namespace cmn {
namespace sprite {

Map::Map() {
}

Map::Map(const Map& other) {
    _props = other._props;
    _print_by_default = other._print_by_default;
}

Map::Map(Map&& other) 
    : _props(std::move(other._props)),
      _print_by_default(std::move(other._print_by_default))
{ }

Map& Map::operator=(const Map& other) {
	_props = other._props;
    _print_by_default = other._print_by_default;
	return *this;
}

Map& Map::operator=(Map&& other) {
	_props = std::move(other._props);
    _print_by_default = std::move(other._print_by_default);
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
        return _type->toStr();
    }
    
    bool Reference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool Reference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }
    
    std::string ConstReference::toStr() const {
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
