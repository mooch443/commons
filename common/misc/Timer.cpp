#include "Timer.h"

#ifdef NDEBUG
//#undef NDEBUG // we currently want the timers to be always-on
#endif

std::string Timer::toStr() const {
    cmn::DurationUS duration{ uint64_t(elapsed() * 1000 * 1000 * 1000) };
    return "Timer<"+cmn::Meta::toStr(duration)+">";
}

TakeTiming::TakeTiming(Timing& t) : timing(t) {
    //timing.start_measure();
    timing.start_();
    timer.reset();
}

void Timing::start_() {
#ifdef TREX_ENABLE_TIMERS
    std::lock_guard<std::mutex> guard(_mutex);
    auto id = std::this_thread::get_id();
    Info& t = _threads[id];
    t.averageTimeSince += t.timer_since.elapsed();
    t.stimingCount++;
    
    for(auto it = _threads.begin(); it != _threads.end();) {
        auto && [tid, info] = *it;
        if (info.timer_since.elapsed() > 10) {
            //cmn::Print("Deleting timer for tid ", &tid," (", _name.c_str(),", ", _threads.size()," threads known)");
            it = _threads.erase(it);
        } else
            ++it;
    }
#endif
}

void Timing::start_measure() {
#ifdef TREX_ENABLE_TIMERS
    std::lock_guard<std::mutex> guard(_mutex);
    auto id = std::this_thread::get_id();
    _threads[id].timer.reset();
    
    for(auto it = _threads.begin(); it != _threads.end();) {
        auto && [tid, info] = *it;
        if (info.timer.elapsed() > 10) {
            //cmn::Print("Deleting timer for tid ", &tid," (", _name.c_str(),", ", _threads.size()," threads known)");
            it = _threads.erase(it);
        } else
            ++it;
    }
#endif
}
double Timing::conclude_measure() {
#ifdef TREX_ENABLE_TIMERS
    double elapsed;
    {
        std::lock_guard<std::mutex> guard(_mutex);
        auto id = std::this_thread::get_id();
        auto &info = _threads[id];
        elapsed = info.timer.elapsed();
    }
    return conclude_measure(elapsed);
#else
    return 0;
#endif
}

double Timing::conclude_measure([[maybe_unused]] double elapsed) {
#ifdef TREX_ENABLE_TIMERS
    std::lock_guard<std::mutex> guard(_mutex);
    auto id = std::this_thread::get_id();
    auto &info = _threads[id];
    
    bool exchange = true;
    if(info.initial_frame.compare_exchange_strong(exchange, false)) {
        cmn::Print("-- (", sample_count,") ", _name," initial frame took ", info.timer.elapsed() * 1000,"ms");
        sample_count++;
        return -1;
    }
    
    info.averageTime += elapsed;
    ++info.timingCount;
    info.elapsed += elapsed;
    
    double all_elapsed = 0;
    for(auto && [id, inf] : _threads) {
        all_elapsed += inf.elapsed;
    }
    
    if (all_elapsed / _threads.size() >= _print_threshold
        && info.timingCount > 0)
    {
        cmn::Print("-- (", sample_count,") ", _name.c_str()," took ", cmn::dec<4>(double(info.averageTime) / double(info.timingCount) * 1000),"ms and ",cmn::dec<4>(info.stimingCount == 0 ? 0.0 : double(info.averageTimeSince) / double(info.stimingCount)*1000.0),"ms between");
        
        for(auto && [id, inf] : _threads) {
            if(inf.timingCount > 0) {
                inf.averageTime /= (double)inf.timingCount;
                inf.timingCount = 1;
                inf.elapsed = 0;
            }
        }
    }
    
    info.timer_since.reset();
    
    return elapsed * 1000;
#else
    return 0;
#endif
}
