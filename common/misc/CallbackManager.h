#pragma once

#include <commons.pc.h>

namespace cmn {

template<typename Argument = std::string_view>
class CallbackManagerImpl {
    template<typename A>
    struct Fn {
        using type = std::function<void(A)>;
    };

    template<>
    struct Fn<void> {
        using type = std::function<void()>;
    };
    
    using Fn_t = typename Fn<Argument>::type;
    
private:
    std::size_t _nextID = 0; // Counter for generating unique IDs
    std::unordered_map<std::size_t, Fn_t> _callbacks;
    mutable std::mutex _mutex; // Mutex to ensure thread-safety
    
public:
    // Register a new callback and return its unique ID
    std::size_t registerCallback(const Fn_t& callback) {
        std::unique_lock lock(_mutex); // Lock the mutex
        std::size_t currentID = _nextID++;
        _callbacks[currentID] = callback;
        return currentID;
    }

    // Unregister (remove) a callback using its unique ID
    void unregisterCallback(std::size_t id) {
        std::unique_lock lock(_mutex); // Lock the mutex
        _callbacks.erase(id);
    }

    // Call all registered callbacks
    template<typename A = Argument>
        requires (not std::same_as<A, void> && std::convertible_to<A, Argument>)
    void callAll(A&& arg) const {
        // Make a copy of the callbacks to avoid potential deadlocks
        decltype(_callbacks) callbacksCopy;
        {
            std::unique_lock lock(_mutex); // Lock the mutex
            callbacksCopy = _callbacks;
        }
        
        for (const auto& [id, callback] : callbacksCopy) {
            callback(std::forward<A>(arg));
        }
    }
    
    // Call all registered callbacks
    template<typename A = Argument>
        requires (std::same_as<A, void>) && (_clean_same<A, Argument>)
    void callAll() const {
        // Make a copy of the callbacks to avoid potential deadlocks
        decltype(_callbacks) callbacksCopy;
        {
            std::unique_lock lock(_mutex); // Lock the mutex
            callbacksCopy = _callbacks;
        }
        
        for (const auto& [id, callback] : callbacksCopy) {
            callback();
        }
    }
};

using CallbackManager = CallbackManagerImpl<std::string_view>;

}
