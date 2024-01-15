#pragma once
#include <commons.pc.h>

namespace cmn {
template<typename... ArgType>
class TaskQueue {
public:
    using TaskFunction = std::function<void(ArgType...)>;
    
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
    }

    template<typename F>
    auto enqueue(F&& f) -> std::future<void> {
        auto task = std::make_shared<std::packaged_task<void(ArgType...)>>(
            std::forward<F>(f)
        );

        std::future<void> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_stop) {
                throw std::runtime_error("enqueue on stopped TaskQueue");
            }
            _tasks.emplace([task](ArgType&& ...arg) { (*task)(std::forward<ArgType>(arg)...); });
        }
        _cond.notify_one();
        return res;
    }

    // Call this with the specific argument you want to pass to the tasks
    void processTasks(ArgType&& ...arg) {
        std::unique_lock<std::mutex> lock(_mutex);
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
    }

private:
    bool _stop;
    std::queue<TaskFunction> _tasks;
    std::mutex _mutex;
    std::condition_variable _cond;
};
}

