#include "ThreadPool.h"
#include <misc/metastring.h>

namespace cmn {
    GenericThreadPool::GenericThreadPool(size_t nthreads, const std::string& thread_prefix, std::function<void(std::exception_ptr)> handle_exceptions, std::function<void()> init)
            : _exception_handler(handle_exceptions),
            nthreads(0), stop(false), _init(init), _working(0), _thread_prefix(thread_prefix+"("+Meta::toStr(nthreads)+")")
    {
        resize(nthreads);
    }

    void GenericThreadPool::force_stop() {
        {
            std::unique_lock<std::mutex> lock(m);
            stop = true;
            condition.notify_all();
        }
        
        for (auto &t : thread_pool) {
            t->join();
            delete t;
        }
        
        thread_pool.clear();
    }

    void GenericThreadPool::resize(size_t num_threads) {
        std::unique_lock<std::mutex> lock(m);
        
        if(num_threads < nthreads) {
            size_t i=(nthreads - num_threads);
            for(auto it = thread_pool.begin() + i; it != thread_pool.end(); ++i) {
                stop_thread.at(i) = true;
                (*it)->join();
                delete *it;
                it = thread_pool.erase(it);
            }
            stop_thread.resize(num_threads);
            
        } else {
            stop_thread.resize(num_threads);
            
            auto thread = [this](std::function<void()> init, int idx)
            {
                auto name = this->thread_prefix()+"::thread_"+Meta::toStr(idx);
                set_thread_name(name+"::init");
                
                std::unique_lock<std::mutex> lock(m);
                init();
                
                for(;;) {
#ifndef NDEBUG
                    set_thread_name(name+"::idle");
#endif
                    if(!q.empty()) {
                        // set busy!
                        _working++;
                        
                        auto task = std::move(q.front());
                        q.pop();
#ifndef NDEBUG
                        set_thread_name(name);
#endif
                        
                        // free mutex, perform task, regain mutex
                        lock.unlock();
                        try {
                            task();
                            lock.lock();
                        } catch(...) {
                            FormatExcept("Exception in GenericThreadPool(", thread_prefix(),").");
                            lock.lock();
                            if (_exception_handler)
                                _exception_handler(std::current_exception());
                            else {
                                _working--;
                                finish_condition.notify_one();
                                throw;
                            }
                        }
                        
                        // not busy anymore!
                        _working--;
                        finish_condition.notify_one();
                        
                    } else if(stop || stop_thread.at(idx)) {
                        break;
                    } else {
                        auto status = condition.wait_for(lock, std::chrono::seconds(10));
                        if(status == std::cv_status::timeout
                           && q.empty() && stop)
                        {
                            FormatWarning("Needed to potentially wait forever on this lock in ", thread_prefix(),"!");
                        }
                    }
                }
            };
            
            while(thread_pool.size() < num_threads) {
                size_t i = thread_pool.size();
                thread_pool.push_back(new std::thread(thread, _init, i));
            }
        }
        
        nthreads = num_threads;
    }

    void GenericThreadPool::wait_one() {
        std::unique_lock<std::mutex> lock(m);
        if(_working == 0 || q.empty())
            return;
        auto previous = q.size();
        finish_condition.wait(lock, [&](){ return q.size() < previous || q.empty() || !_working; });
    }
}
