#pragma once
#include <commons.pc.h>

#if !defined(NDEBUG) && false
#define CTIMER_ENABLED true
#else
#define CTIMER_ENABLED false
#endif

#if CTIMER_ENABLED
#include <misc/Timer.h>
#endif

struct CTimer {
#if CTIMER_ENABLED
    struct Timing {
        double count{0};
        uint64_t samples{0};
        
        std::string toStr() const {
            return cmn::dec<4>(count / double(samples) * 1000).toStr()+"ms total = "+ cmn::dec<4>(count).toStr()+"s";
        }
    };
    
    static inline std::unordered_map<std::string, Timing> timings;
    Timer timer;
#endif
    [[maybe_unused]] std::string_view _unused;
#if CTIMER_ENABLED
    ~CTimer() {
        auto elapsed = timer.elapsed();
        auto &field = timings[std::string(_name)];
        field.count += elapsed;
        field.samples++;
        
        if(field.samples % 10000 == 0) {
            cmn::Print("! [", _name, "] ", field);
            field.count /= double(field.samples);
            field.samples = 1;
        }
    }
#endif
};
