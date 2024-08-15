#pragma once

#include <commons.pc.h>

namespace cmn {

// Policy classes
struct ThreadSafePolicy {
    using mutex_type = std::mutex;
    using lock_type = std::unique_lock<mutex_type>;
};

struct NonThreadSafePolicy {
    struct dummy_mutex {
        void lock() {}
        void unlock() {}
    };
    struct dummy_lock {
        dummy_lock(dummy_mutex&) {}
    };
    using mutex_type = dummy_mutex;
    using lock_type = dummy_lock;
};

// Single class managing the object cache
template <  typename T,
            size_t maxCacheSize = 0,
            template <typename...> class PtrType = std::unique_ptr,
            typename ThreadPolicy = NonThreadSafePolicy  >
class ObjectCache {
    using Ptr = PtrType<T>;
public:
    // Destructor that clears the cache
    ~ObjectCache() {
        clearCache();
    }
    
    // Method to get an object from the cache
    Ptr getObject() {
        typename ThreadPolicy::lock_type lock{cacheMutex};
        if (not cache.empty()) {
            auto obj = std::move(cache.back());
            cache.pop_back();
            return obj;
        }
        
        return std::make_unique<T>();
    }
    
    // Method to return an object to the cache
    void returnObject(Ptr&& obj) {
        typename ThreadPolicy::lock_type lock{cacheMutex};
        if (maxCacheSize > 0 && cache.size() >= maxCacheSize) {
            return;
        }
        
        cache.push_back(std::move(obj));
    }
    
    // Method to clear the cache
    void clearCache() {
        typename ThreadPolicy::lock_type lock{cacheMutex};
        cache.clear();
    }
    
private:
    std::vector<Ptr> cache;
    typename ThreadPolicy::mutex_type cacheMutex;
};

}
