#pragma once
#include <commons.pc.h>
#include <misc/PackLambda.h>

namespace cmn {
template<typename... ArgType>
class TaskQueue {
public:
    using TaskFunction = package::promised<void, ArgType...>;
    
    TaskQueue() : _stop(false) {}
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    ~TaskQueue() {
        stop();
    }

    void stop() {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _stop = true;
        }
        _cond.notify_all();
        
        std::vector<std::shared_future<void>> futures;
        {
            std::unique_lock lock{_mutex};
            futures = std::move(_owned_completions);
            _owned_completions.clear();
        }
        
        for (auto& future : futures) {
            try {
                future.get();
            } catch (const std::exception& ex) {
                FormatError("Background task failed: ", ex.what());
            } catch (...) {
                FormatError("Background task failed with an unknown exception.");
            }
        }
    }

    template<typename F>
    auto enqueue(F&& f) -> std::future<void> {
        //auto task = pack<void>(std::forward<F>(f));
        /*auto task = std::make_unique<std::packaged_task<void(ArgType...)>>(
            std::forward<F>(f)
        );*/
        TaskFunction task(std::forward<F>(f));
        std::future<void> res = task.get_future();
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_stop) {
                throw std::runtime_error("enqueue on stopped TaskQueue");
            }
            _tasks.emplace([task = std::move(task)](ArgType ...arg) { task(arg...);
            });
        }
        _cond.notify_one();
        return res;
    }

    // Call this with the specific argument you want to pass to the tasks
    bool processTasks(ArgType ...arg) {
        std::unique_lock lock{_mutex};
        bool ret = not _tasks.empty();
        
        while (not _stop && not _tasks.empty()) {
            TaskFunction task = std::move(_tasks.front());
            _tasks.pop();
            
            lock.unlock();
            try {
                task(std::forward<ArgType>(arg)...);
            } catch(const std::exception& ex) {
                FormatExcept("Ignoring exception in main queue task: ", ex.what());
            }
            
            lock.lock();
            
            if (_stop || _tasks.empty()) {
                break;
            }
            _cond.wait(lock, [this]() {
                return !_tasks.empty() || _stop;
            });
        }
        
        /// clear some items if possible
        for(auto it = _owned_completions.begin(); it != _owned_completions.end(); )
        {
            if(cmn::is_future_ready(*it))
            {
                try {
                    it->get();
                } catch (const std::exception& ex) {
                    FormatError("Background task failed: ", ex.what());
                } catch (...) {
                    FormatError("Background task failed with an unknown exception.");
                }
                
                it = _owned_completions.erase(it);
            } else
                ++it;
        }
        
        return ret;
    }
    
    void clear() {
        std::unique_lock<std::mutex> lock(_mutex);
        while (not _tasks.empty())
            _tasks.pop();
        _cond.notify_all();
    }
    
    /// @brief  Add a future to the list of owned completions, which will be waited for at least whenever processTasks is called or we destroy our queue.
    /// @param f  The shared future representing the completion of a background task.
    void push_owned_completion(std::shared_future<void> f) {
        std::unique_lock lock{_mutex};
        _owned_completions.push_back(std::move(f));
    }

private:
    bool _stop;
    std::queue<TaskFunction> _tasks;
    std::vector<std::shared_future<void>> _owned_completions;
    std::mutex _mutex;
    std::condition_variable _cond;
};
}

