#pragma once

#include <commons.pc.h>

namespace cmn {
class CallbackManager {
private:
    std::size_t _nextID = 0; // Counter for generating unique IDs
    std::unordered_map<std::size_t, std::function<void(std::string_view)>> _callbacks;
    mutable std::mutex _mutex; // Mutex to ensure thread-safety
    
public:
    // Register a new callback and return its unique ID
    std::size_t registerCallback(const std::function<void(std::string_view)>& callback);
    
    // Unregister (remove) a callback using its unique ID
    void unregisterCallback(std::size_t id);
    
    // Call all registered callbacks
    void callAll(std::string_view arg) const;
};
}
