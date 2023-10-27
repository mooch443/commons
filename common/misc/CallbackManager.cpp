#include "CallbackManager.h"

namespace cmn {

// Register a new callback and return its unique ID
std::size_t CallbackManager::registerCallback(const std::function<void(std::string_view)>& callback) {
    std::unique_lock lock(_mutex); // Lock the mutex
    std::size_t currentID = _nextID++;
    _callbacks[currentID] = callback;
    return currentID;
}

// Unregister (remove) a callback using its unique ID
void CallbackManager::unregisterCallback(std::size_t id) {
    std::unique_lock lock(_mutex); // Lock the mutex
    _callbacks.erase(id);
}

// Call all registered callbacks
void CallbackManager::callAll(std::string_view arg) const {
    // Make a copy of the callbacks to avoid potential deadlocks
    decltype(_callbacks) callbacksCopy;
    {
        std::unique_lock lock(_mutex); // Lock the mutex
        callbacksCopy = _callbacks;
    }
    
    for (const auto& [id, callback] : callbacksCopy) {
        callback(arg);
    }
}

}
