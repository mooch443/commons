#pragma once
#include <commons.pc.h>

namespace cmn {

template<typename T>
class ObjectManager {
public:
    using ID = std::size_t;
private:
    ID _nextID = 0; // Counter for generating unique IDs
    std::unordered_map<ID, T> _objects;
    mutable std::mutex _mutex; // Mutex to ensure thread-safety
    
public:
    // Register a new object and return its unique ID
    ID registerObject(T&& object) {
        std::unique_lock lock(_mutex); // Lock the mutex
        ID currentID = _nextID++;
        _objects[currentID] = std::move(object);
        return currentID;
    }

    // Unregister (remove) an object using its unique ID
    void unregisterObject(ID id) {
        std::unique_lock lock(_mutex); // Lock the mutex
        _objects.erase(id);
    }
    
    bool hasObject(ID id) const noexcept {
        std::unique_lock lock(_mutex); // Lock the mutex
        return _objects.find(id) != _objects.end();
    }

    // Get a registered object by its unique ID
    const T& getObject(ID id) const {
        std::unique_lock lock(_mutex); // Lock the mutex
        auto it = _objects.find(id);
        if (it != _objects.end()) {
            return it->second;
        }
        throw InvalidArgumentException("Cannot find ID ", id, " in manager.");
    }

    // Get a registered object by its unique ID
    T& getObject(ID id) {
        std::unique_lock lock(_mutex); // Lock the mutex
        auto it = _objects.find(id);
        if (it != _objects.end()) {
            return it->second;
        }
        throw InvalidArgumentException("Cannot find ID ", id, " in manager.");
    }

    // Get all registered objects
    std::unordered_map<ID, T> getAllObjects() const {
        std::unique_lock lock(_mutex); // Lock the mutex
        return _objects; // Return a copy of the objects map
    }
};

}
