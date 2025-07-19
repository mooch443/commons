#pragma once

#include <commons.pc.h>

#include <misc/Timer.h>
#include <misc/PackLambda.h>

namespace cmn {
    class GenericThreadPool {
        std::queue< package::F<void()> > q;
        std::mutex m;
        
        std::condition_variable condition;
        std::condition_variable finish_condition;
        std::vector<std::thread*> thread_pool;
        std::vector<bool> stop_thread;
        std::function<void(std::exception_ptr)> _exception_handler;
        size_t nthreads;
        std::atomic_bool stop;
        std::function<void()> _init;
        mutable std::mutex _thread_id_mutex;
        
        GETTER(std::atomic_int, working);
        GETTER(std::string, thread_prefix);
        std::vector<std::thread::id> _thread_ids;
        
    public:
        GenericThreadPool(size_t nthreads, const std::string& thread_prefix, std::function<void(std::exception_ptr)> handle_exceptions = nullptr, std::function<void()> init = [](){});
        
        ~GenericThreadPool() {
            force_stop();
        }
        
        size_t num_threads() const {
            return nthreads;
        }
        
        std::vector<std::thread::id> thread_ids() const;
        
        void resize(size_t num_threads);
        
        size_t queue_length() {
            std::unique_lock<std::mutex> lock(m);
            return q.size();
        }
        
        template<class F, class... Args>
        auto enqueue(F f, Args... args) -> std::future<typename std::invoke_result_t<F, Args...>>
        {
            using return_type = typename std::invoke_result_t<F, Args...>;
            //auto task = cmn::package::promised<return_type, Args...>(std::forward<F>(f));
            //auto task = std::make_shared< std::packaged_task<return_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::promise<return_type> promise;
            std::future<return_type> res = promise.get_future();
            auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            
            {
                std::unique_lock lock(m);
                
                // don't allow enqueueing after stopping the pool
                if(stop) {
                    throw U_EXCEPTION("enqueue on stopped ThreadPool");
                }
                
                q.push(package::F<void()>([bound = std::move(bound), promise = std::move(promise)]() mutable {
                    try {
                        if constexpr(std::same_as<return_type, void>) {
                            //f(std::forward<Args>(args)...);
                            bound();
                            promise.set_value();
                        } else {
                            promise.set_value(bound());
                            //promise.set_value(f(std::forward<Args>(args)...));
                        }
                    } catch(...) {
                        promise.set_exception(std::current_exception());
                    }
                }));
            }
            
            condition.notify_one();
            return res;
        }
        
        void force_stop();
        
        void wait() {
            std::unique_lock<std::mutex> lock(m);
            if(q.empty() && (_working == 0))
                return;
            finish_condition.wait(lock, [&](){ return q.empty() && (_working == 0); });
        }
        
        void wait_one();
    };

template <typename F, typename Tuple, std::size_t i, std::size_t... I>
constexpr bool check_invocability_with_void_ptr_impl(std::index_sequence<I...>) {
    return (std::invocable<F, std::conditional_t<I == i, void*, std::tuple_element_t<I, Tuple>>...>);
}

template <typename F, typename ArgsTuple, std::size_t i>
consteval bool is_generic_lambda() {
    constexpr auto size = std::tuple_size_v<ArgsTuple>;
    return []<std::size_t... I>(std::index_sequence<I...>) -> bool {
        return (std::invocable<F, std::conditional_t<I == i, void*, std::tuple_element_t<I, ArgsTuple>>...>);
        //return (check_invocability_with_void_ptr_impl<F, ArgsTuple, I>(sequence) || ...);
    }(std::make_index_sequence<size>{});
}

template<std::size_t i, typename DefaultType, typename... Args>
struct conditional_conversion {
    template<typename F, typename Value>
        requires (is_generic_lambda<F, std::tuple<Args...>, i>())
    static constexpr DefaultType convert(Value value, F* = nullptr) {
        return static_cast<DefaultType>(value);
    }

    template<typename F, typename Value>
        requires (not is_generic_lambda<F, std::tuple<Args...>, i>())
    static constexpr auto convert(Value value, F* = nullptr) {
        return narrow_cast<std::tuple_element_t<i, typename detail::arg_types<F>::type>>(value);
    }
};

template<typename F, typename Iterator, typename Pool>
void distribute_indexes(F&& fn, Pool& pool, Iterator start, Iterator end, uint32_t threads = 0) {
    if(threads == 0)
        threads = (uint32_t)pool.num_threads();
    
    /// calculate how many items we have
    /// been given by caller:
    int64_t N;
    if constexpr(std::integral<Iterator>)
        N = end - start;
    else
        N = std::distance(start, end);
    
    /// catch special cases
    /// if no items are provided, exit.
    if(N <= 0)
        return;
    
    /// if only one thread is provided, just run
    /// the function and be done with it
    if(threads <= 1) {
        fn(0, start, end, 0);
        return;
    }
    
    /// otherwise we need to actually calculate how many threads
    /// to run and how many items / thread we want
    const int64_t per_thread = max(1, int64_t(N) / int64_t(threads));
    
    //Print("Distributing ", N, " elements over ", threads, " threads (", per_thread, " per thread with ", int64_t(N) % int64_t(threads),")");
    
    int64_t i=0;
    size_t j=0;
    Iterator nex = start;
    std::vector<std::future<void>> futures(min(N, static_cast<int64_t>(threads)) - 1);
    
    for(auto it = start; it != end; ++j) {
        int64_t step;
        if(j + 1 == threads) {
            step = N - i;
            
        } else {
            /// there is the current package, plus a full package left
            step = per_thread;
        }
        
        if constexpr(std::integral<Iterator>)
            nex += step;
        else
            std::advance(nex, step);
        
        if(nex == end) {
            // run in local thread
            fn(i, it, nex, conditional_conversion<4, size_t, int64_t, Iterator, Iterator, size_t>::template convert<std::remove_cvref_t<F>>(j));
            
        } else {
            futures[j] = pool.enqueue([&](int64_t i, auto it, auto nex, size_t index) {
                fn(i, it, nex, conditional_conversion<4, size_t, int64_t, Iterator, Iterator, size_t>::template convert<std::remove_cvref_t<F>>(index));
            }, i, it, nex, j);
        }
        
        it = nex;
        i += step;
    }
    
    assert(i == N);
    for(auto &f : futures)
        f.get();
}

    template<typename T>
    class QueueThreadPool {
        std::queue<T> q;
        std::mutex m;
        
        std::condition_variable condition;
        std::condition_variable finish_condition;
        std::vector<std::thread*> thread_pool;
        const size_t nthreads;
        std::atomic_bool stop{false};
        std::atomic_int _working{0};
        std::function<void(T&)> work_function;
        
    public:
        QueueThreadPool(size_t nthreads, const std::function<void(T&)>& fn, const std::string& prefix = "QueueThreadPool")
            : nthreads(nthreads), stop(false), _working(0), work_function(fn)
        {
            for (size_t i=0; i<nthreads; i++) {
                thread_pool.push_back(new std::thread([&, p = prefix](int idx){
                    std::unique_lock<std::mutex> lock(m);
                    cmn::set_thread_name(p+"::thread_"+cmn::Meta::toStr(idx));
                    
                    for(;;) {
                        condition.wait(lock, [&](){ return !q.empty() || stop; });
                        
                        if(q.empty() && stop)
                            break;
                        if(q.empty())
                            continue;
                        
                        auto item = std::move(q.front());
                        q.pop();
                        ++_working;
                        
                        lock.unlock();
                        try {
                            work_function(item);
                        } catch(...) {
                            FormatExcept("Exception caught in work item of thread ", cmn::get_thread_name());
                        }
                        lock.lock();
                        
                        --_working;
                        finish_condition.notify_one();
                    }
                    
                }, i));
            }
        }
        
        ~QueueThreadPool() {
            stop = true;
            condition.notify_all();
            
            for (auto &t : thread_pool) {
                t->join();
                delete t;
            }
        }

        size_t num_threads() const {
            return nthreads;
        }
        
        void enqueue(T obj) {
            {
                std::unique_lock<std::mutex> lock(m);
                q.push(obj);
            }
            
            condition.notify_one();
        }
        
        void wait() {
            std::unique_lock<std::mutex> lock(m);
            {
                finish_condition.wait(lock, [&](){ return q.empty() && _working.load() == 0; });
            }
        }
    };

    class SimpleThreadPool {
        size_t max_size;
        std::queue<std::thread*> threads;
        
    public:
        SimpleThreadPool(size_t nthreads) : max_size(nthreads)
            {}
        
        void enqueue(std::thread*ptr) {
            while (threads.size() >= max_size) {
                threads.front()->join();
                delete threads.front();
                threads.pop();
            }
            
            threads.push(ptr);
        }
        
        void wait() {
            while (!threads.empty()) {
                threads.front()->join();
                delete threads.front();
                threads.pop();
            }
        }
        
        ~SimpleThreadPool() {
            wait();
        }
    };

    class ThreadPool {
    public:
        ThreadPool(size_t);
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F(Args...)>>;
        ~ThreadPool();
    private:
        // need to keep track of threads so we can join them
        std::vector< std::thread > workers;
        // the task queue
        std::queue< std::function<void()> > tasks;
        
        // synchronization
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
    };

    // the constructor just launches some amount of workers
    inline ThreadPool::ThreadPool(size_t threads)
    :   stop(false)
    {
        for(size_t i = 0;i<threads;++i) {
            workers.emplace_back(
             [this] {
                 for(;;)
                 {
                     std::function<void()> task;
                     
                     {
                         std::unique_lock<std::mutex> lock(this->queue_mutex);
                         this->condition.wait(lock,
                                              [this]{ return this->stop || !this->tasks.empty(); });
                         if(this->stop && this->tasks.empty())
                             return;
                         if(this->tasks.empty())
                             continue;
                         
                         task = std::move(this->tasks.front());
                         this->tasks.pop();
                     }
                     
                     task();
                 }
             });
        }
    }

    // add new work item to the pool
    template<class F, class... Args>
    auto ThreadPool::enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F(Args...)>>
    {
        using return_type = typename std::invoke_result_t<F(Args...)>;
        
        auto task = std::make_shared< std::packaged_task<return_type()> >(
                                                                          std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                                                                          );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // don't allow enqueueing after stopping the pool
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    // the destructor joins all threads
    inline ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }

}
