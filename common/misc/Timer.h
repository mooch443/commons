#ifndef M_TIMER_H
#define M_TIMER_H

#include <commons.pc.h>

namespace cmn {
class MetaObject;
}

class Timer
{
public:
    Timer() : beg_(clock_::now()) {}
    void reset() { beg_ = clock_::now(); }
    double elapsed() const { 
        return std::chrono::duration_cast<second_>
            (clock_::now() - beg_).count(); }
    double stop() {
        double e = elapsed();
        reset();
        return e;
    }
    
    std::string toStr() const;
    static std::string class_name() { return "Timer"; }

private:
    typedef std::chrono::high_resolution_clock clock_;
    typedef std::chrono::duration<double, std::ratio<1> > second_;
    std::chrono::time_point<clock_> beg_;
};

class Timing {
    //Timer _timer;
    std::string _name;
    const double _print_threshold;
    long sample_count;
    
    struct Info {
        Timer timer, timer_since;
        double averageTime, averageTimeSince{0.0};
        double elapsed;
        long timingCount, stimingCount{0};
        std::atomic_bool initial_frame;
        
        Info() : averageTime(0.0), elapsed(0.0), timingCount(0), initial_frame(false) {}
    };
    
    std::mutex _mutex;
    std::map<std::thread::id, Info> _threads;
    
public:
    Timing(const std::string& name, double print_threshold = 1.0)
        : _name(name), _print_threshold(print_threshold), sample_count(0)
    {
        
    }
    
    void start_measure();
    void start_();
    double conclude_measure();
    double conclude_measure(double elapsed);
};

class TakeTiming {
    Timing &timing;
    Timer timer;
    
public:
    TakeTiming(Timing& t);
    
    ~TakeTiming() {
        timing.conclude_measure(timer.elapsed());
        
    }
};

#endif
