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
    *this = other;
}

Map::Map(Map&& other) 
    : _props(std::move(other._props)),
    _id_counter(other._id_counter.load()),
    _shutdown_callbacks(std::move(other._shutdown_callbacks)),
    _general_callbacks(std::move(other._general_callbacks)),
    _before_write_callbacks(std::move(other._before_write_callbacks)),
    _registered_general_callbacks(std::move(other._registered_general_callbacks)),
    _registered_before_write_callbacks(std::move(other._registered_before_write_callbacks)),
    _pending_callbacks(std::move(other._pending_callbacks)),
    _print_by_default(std::move(other._print_by_default))
{ }

Map& Map::operator=(const Map& other) {
    if(this == &other)
        return *this;

    const auto print_by_default = _print_by_default;
    _print_by_default = false;
    
    auto other_keys = other.keys();
    auto my_keys = keys();
    std::vector<bool> found_keys(other_keys.size(), false);
    std::vector<bool> my_found_keys(my_keys.size(), false);
    
    for(size_t i = 0; i < other_keys.size(); ++i) {
        for(size_t j = 0; j < my_keys.size(); ++j) {
            if(other_keys[i] == my_keys[j]) {
                found_keys[i] = true;
                break;
            }
        }
    }
    
    for(size_t i = 0; i < my_keys.size(); ++i) {
        for(size_t j = 0; j < other_keys.size(); ++j) {
            if(my_keys[i] == other_keys[j]) {
                my_found_keys[i] = true;
                break;
            }
        }
    }
    
    for(size_t i = 0; i < my_keys.size(); ++i) {
        if(not my_found_keys[i]) {
            _props.erase(my_keys[i]);
        }
    }

    std::unordered_map<std::string_view, CallbackManager> callbacks;
    std::unordered_map<std::string_view, CallbackManager> before_write_callbacks;
    std::set<std::string_view> need_to_copy;
    
    for(size_t i = 0; i < other_keys.size(); ++i) {
        if(found_keys[i]) {
            // Handle keys that are in both maps (i.e. need update callbacks)
            if(*other._props.at(other_keys[i]) != *_props.at(other_keys[i]))
            {
                callbacks[other_keys[i]] = _props.at(other_keys[i])->_callbacks;
                before_write_callbacks[other_keys[i]] = _props.at(other_keys[i])->_before_write_callbacks;
                need_to_copy.insert(other_keys[i]);
            }
            
        } else {
            need_to_copy.insert(other_keys[i]);
        }
    }
    
    for(auto& [name, callback] : callbacks) {
        _props.at(name)->_callbacks = {};
    }
    
    auto general_callbacks = std::move(_general_callbacks);
    _general_callbacks.clear();
    auto pending_callbacks = std::move(_pending_callbacks);
    _pending_callbacks.clear();
    std::vector<std::pair<Store, std::size_t>> deferred_insert_callbacks;
    std::exception_ptr error;
    try {
        for(std::string_view name : need_to_copy) {
            other._props.at(name)->copy_to(*this);
        }
    } catch(...) {
        error = std::current_exception();
    }
    
    _general_callbacks = std::move(general_callbacks);
    for(size_t i = 0; i < other_keys.size(); ++i) {
        if(not found_keys[i] && _props.contains(other_keys[i])) {
            auto name = other_keys[i];
            for(auto &fn : _general_callbacks) {
                auto id = _props.at(name)->registerCallback(fn);
                _registered_general_callbacks.emplace_back(name, id);
                deferred_insert_callbacks.emplace_back(_props.at(name), id);
            }
        }
    }
    
    for(auto& [name, callback] : callbacks) {
        if(_props.contains(name)) {
            _props.at(name)->_callbacks = callback;
            _props.at(name)->_before_write_callbacks = before_write_callbacks.at(name);
        }
    }

    for(auto& [name, pending] : pending_callbacks) {
        auto& target = _pending_callbacks[name];
        target.insert(target.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
    }
    for(auto it = _pending_callbacks.begin(); it != _pending_callbacks.end();) {
        if(not _props.contains(it->first)) {
            ++it;
            continue;
        }
        auto property = _props.at(it->first);
        for(auto& future : it->second) {
            auto cb = property->registerCallback(future.fn);
            future.addition._ids[it->first] = cb;
            --(*future.remaining);
            if(*future.remaining == 0)
                future.prom->set_value(std::move(future.addition));
            if(future.init_type == RegisterInit::DO_TRIGGER)
                deferred_insert_callbacks.emplace_back(property, cb);
        }
        it = _pending_callbacks.erase(it);
    }
    
    _print_by_default = error ? print_by_default : other._print_by_default;
    if(error)
        std::rethrow_exception(error);

    for(std::string_view name : need_to_copy) {
        if(callbacks.contains(name))
            _props.at(name)->triggerCallbacks();
    }
    for(auto& [property, id] : deferred_insert_callbacks) {
        property->triggerCallback(id);
    }
	return *this;
}

Map& Map::operator=(Map&& other) {
    if(this == &other) {
        return *this;
    }

	_props = std::move(other._props);
    _print_by_default = std::move(other._print_by_default);
    _shutdown_callbacks = std::move(other._shutdown_callbacks);
    _id_counter = other._id_counter.load();
    _general_callbacks = std::move(other._general_callbacks);
    _before_write_callbacks = std::move(other._before_write_callbacks);
    _registered_general_callbacks = std::move(other._registered_general_callbacks);
    _registered_before_write_callbacks = std::move(other._registered_before_write_callbacks);
    _pending_callbacks = std::move(other._pending_callbacks);
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
        if(auto ptr = _type.lock();
           not ptr)
        {
            return "null";
        } else {
            return ptr->toStr();
        }
    }
    
    bool Reference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool Reference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }

bool Reference::valid() const {
    auto ptr = _type.lock();
    return ptr && ptr->valid();
    /*if (_container && (not _container->has(_name) || &_container->at(_name).get() != _type))
        return false;//throw InvalidArgumentException("Reference to an invalid option.");
    return _type && _type->valid();*/
}

bool ConstReference::valid() const {
    auto ptr = _type.lock();
    return ptr && ptr->valid();
}

std::string_view Reference::type_name() const {
    if(auto ptr = _type.lock();
       not ptr)
    {
        throw InvalidArgumentException("Reference to an invalid option.");
    } else
        return ptr->type_name();
}
std::string_view ConstReference::type_name() const {
    auto ptr = _type.lock();
    if(not ptr)
        throw InvalidArgumentException("Reference to an invalid option.");
    return ptr->type_name();
}
    
    std::string ConstReference::toStr() const {
        auto ptr = _type.lock();
        if(not ptr)
            return "null";
        return ptr->toStr();
    }
    
    bool ConstReference::operator==(const PropertyType& other) const {
        return get().operator==(other);
    }
    
    bool ConstReference::operator!=(const PropertyType& other) const {
        return get().operator!=(other);
    }

Reference::Reference(Map& container, std::weak_ptr<PropertyType> type)
    : _type(type), _container(&container)
{ }

std::string Reference::name() const {
    if(_name)
        return _name.value();
    
    auto lock = _type.lock();
    if(not lock)
        throw std::runtime_error("Property we are referencing in name() does not exist anymore.");
    return std::string(lock->name());
}

}
}
