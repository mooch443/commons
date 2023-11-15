#pragma once
#include <commons.pc.h>

namespace cmn {

template<typename T, typename Construct>
struct Buffers {
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }
    inline static std::vector<T> _buffers;
    inline static Construct _create{};
    
    static T get() {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
            _buffers.pop_back();
            return ptr;
        }
        
        return _create();
    }
    
    static void move_back(T&& image) {
        std::unique_lock guard(mutex());
        if(image)
            _buffers.emplace_back(std::move(image));
    }
};

}
