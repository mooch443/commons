#include "SFLoop.h"
#include <misc/GlobalSettings.h>
#include <gui/DrawBase.h>
#include <misc/SpriteMap.h>

namespace cmn::gui {
    SFLoop::SFLoop(DrawStructure& graph, Base* base,
           const std::function<void(SFLoop&, gui::LoopStatus)>& custom_loop,
           const std::function<void(SFLoop&)>& after_display,
           const std::function<void(SFLoop&)>& idle_callback)
        : _base(base), _graph(graph), _after_display(after_display), _idle_callback(idle_callback), _please_end(false)
    {
        SETTING(terminate) = false;
        bool _do_terminate = false;
        
        GlobalSettings::map().register_callbacks({"terminate"}, [&_do_terminate](auto) {
            _do_terminate = SETTING(terminate).value<bool>();
        });
        
        gui::LoopStatus status = gui::LoopStatus::UPDATED;
        
        while (!_do_terminate && !_please_end) {
            tf::show();
            
            if(custom_loop)
                custom_loop(*this, status);
            
            if(_base) {
                status = _base->update_loop();
                if(status == gui::LoopStatus::END) {
                    SETTING(terminate) = true;
                    
                } else if(status != gui::LoopStatus::UPDATED) {
                    if(_idle_callback)
                        _idle_callback(*this);
                    //std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    
                } else {
                    _time_since_last_update.reset();
                }
                
            } else {
                //auto guard = GUI_LOCK(_graph.lock());
                _graph.before_paint((Base*)nullptr);
            }
                
            if(_after_display) {
                //std::unique_lock<std::recursive_mutex> guard(_graph.lock());
                _after_display(*this);
            }
            
            {
                std::lock_guard<std::mutex> guard(queue_mutex);
                while(!main_exec_queue.empty()) {
                    main_exec_queue.front()();
                    main_exec_queue.pop();
                }
            }
        }
    }
    
    void SFLoop::add_to_queue(std::function<void ()> fn) {
        std::lock_guard<std::mutex> guard(queue_mutex);
        main_exec_queue.push(fn);
    }
}
