#pragma once

#include <commons.pc.h>

namespace cmn {

namespace callback {
    template<typename A>
    struct Fn {
        using type = std::function<void(A)>;
    };

    // Specialize Fn for void outside of the CallbackManagerImpl class
    template<>
    struct Fn<void> {
        using type = std::function<void()>;
    };
}

template<typename Argument = std::string_view>
class CallbackManagerImpl {
    using Fn_t = typename callback::Fn<Argument>::type;
    
private:
    std::size_t _nextID = 1; // Counter for generating unique IDs
    std::unordered_map<std::size_t, Fn_t> _callbacks;
    mutable std::mutex _mutex; // Mutex to ensure thread-safety
    
public:
    
    CallbackManagerImpl() = default;
    CallbackManagerImpl(CallbackManagerImpl&& other)
    {
        std::scoped_lock guard(other._mutex);
        _nextID = std::move(other._nextID);
        _callbacks = std::move(other._callbacks);
    }
    CallbackManagerImpl(const CallbackManagerImpl& other)
    {
        std::scoped_lock guard(other._mutex);
        _nextID = other._nextID;
        _callbacks = other._callbacks;
    }
    
    CallbackManagerImpl& operator=(CallbackManagerImpl&& other) {
        std::scoped_lock guard(_mutex, other._mutex);
        _nextID = std::move(other._nextID);
        _callbacks = std::move(other._callbacks);
        return *this;
    }
    CallbackManagerImpl& operator=(const CallbackManagerImpl& other) {
        std::scoped_lock guard(_mutex, other._mutex);
        _nextID = other._nextID;
        _callbacks = other._callbacks;
        return *this;
    }
    
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
        if(auto it = _callbacks.find(id);
           it != _callbacks.end())
        {
            _callbacks.erase(it);
        } else {
#ifndef NDEBUG
            FormatWarning("Callbacks did not contain ID ", id, ".");
#endif
        }
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
        requires (not std::same_as<A, void> && std::convertible_to<A, Argument>)
    void call(std::size_t id, A&& arg) const {
        // Make a copy of the callbacks to avoid potential deadlocks
        Fn_t callbackCopy;
        {
            std::unique_lock lock(_mutex); // Lock the mutex
            if(auto it = _callbacks.find(id);
               it != _callbacks.end()) 
            {
                callbackCopy = it->second;
            }
        }
        
        if(callbackCopy)
            callbackCopy(std::forward<A>(arg));
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
