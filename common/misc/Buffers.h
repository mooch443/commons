#pragma once
#include <commons.pc.h>

namespace cmn {

template <typename Func, typename Arg>
concept CallableWith = std::is_invocable_v<Func, Arg>;

template<typename T, typename Construct>
struct Buffers {
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }
    std::vector<T> _buffers;
    Construct _create{};
    
    T get(cmn::source_location loc) {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
            _buffers.pop_back();
            return ptr;
        }
        
        if constexpr(std::is_invocable_v<Construct, std::source_location&&>)
            return _create(std::move(loc));
        else
            return _create();
    }
    
    void move_back(T&& image) {
        std::unique_lock guard(mutex());
        if(image)
            _buffers.emplace_back(std::move(image));
    }
};

}
