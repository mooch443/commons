#pragma once

#include "CrossPlatform.h"
#include <gui/DrawBase.h>
#include <misc/Timer.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#endif

#if COMMONS_HAS_OPENGL || defined(__EMSCRIPTEN__)
#include "GLImpl.h"
#endif

#include "MetalImpl.h"

#if !COMMONS_HAS_OPENGL && !COMMONS_METAL_AVAILABLE && !defined(__EMSCRIPTEN__)
static_assert(false, "Need one of those");
#endif

#if COMMONS_METAL_AVAILABLE
using default_impl_t = gui::MetalImpl;
#else
using default_impl_t = gui::GLImpl;
#endif

namespace gui {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
    ENUM_CLASS(Effects, blur)
#pragma clang diagnostic pop

    class IMGUIBase : public Base {
    protected:
        struct baseFunctor {
            virtual void operator()()=0;
            virtual ~baseFunctor() {}
        };
        
        template<typename T>
        class functor : public baseFunctor {
            T f;
        public:
            template<typename U>
            functor(U&& f)
                :    f(std::forward<U>(f))
            {}
            void operator()() override {
                f();
            }
        };
        
        GETTER_NCONST(std::shared_ptr<CrossPlatform>, platform)
        DrawStructure * _graph;
        CrossPlatform::custom_function_t _custom_loop;
        GETTER(Bounds, work_area)
        std::function<void(const gui::Event&)> _event_fn;
        size_t _objects_drawn, _skipped;
        std::unordered_map<Type::Class, size_t> _type_counts;
        Timer _last_debug_print;
        Size2 _last_framebuffer_size;
        float _dpi_scale;
        float _last_dpi_scale = -1;
        std::string _title;
        std::function<bool(const std::vector<file::Path>&)> _open_files_fn;
        
        struct DrawOrder {
            enum Type {
                DEFAULT = 0,
                //POP,
                END_ROTATION,
                START_ROTATION
            };
            Type type = DEFAULT;
            size_t index;
            Drawable* ptr;
            gui::Transform transform;
            Bounds bounds;
            ImVec4 _clip_rect;
            
            DrawOrder() {}
            DrawOrder(Type type, size_t index, Drawable*ptr, const gui::Transform& transform, const Bounds& bounds, const ImVec4& clip)
            : type(type), index(index), ptr(ptr), transform(transform), bounds(bounds), _clip_rect(clip)
            {}
        };
        
        std::unordered_map<Drawable*, std::tuple<int, Vec2>> _rotation_starts;
        std::vector<DrawOrder> _draw_order;
        
        std::mutex _mutex;
        std::queue<std::function<void()>> _exec_main_queue;

#if defined(__EMSCRIPTEN__)
        static void downloadSuccess(emscripten_fetch_t* fetch);
        static void downloadFailed(emscripten_fetch_t* fetch);

        std::condition_variable _download_variable;
        std::mutex _download_mutex;
        std::atomic_bool _download_finished;
        std::vector<char >font_data;
#endif
        
    public:
        template<typename impl_t = default_impl_t>
        IMGUIBase(std::string title, DrawStructure& base, CrossPlatform::custom_function_t custom_loop, std::function<void(const gui::Event&)> event_fn) 
            : _custom_loop(custom_loop), _event_fn(event_fn), _title(title)
        {
            set_graph(base);
            
            auto ptr = new impl_t([this](){
                //! draw loop
                if(_graph == NULL)
                    return;
                
                std::lock_guard<std::recursive_mutex> lock(_graph->lock());
                this->paint(*_graph);
                
                auto cache = _graph->root().cached(this);
                if(cache)
                    cache->set_changed(false);
                
            }, [this]() -> bool {
                //! new frame function, tells the drawing system whether an update is required
                std::lock_guard<std::recursive_mutex> lock(_graph->lock());
                _graph->before_paint(this);
                
                auto cache = _graph->root().cached(this);
                if(!cache) {
                    cache = _graph->root().insert_cache(this, std::make_unique<CacheObject>()).get();
                }
                
                return cache->changed();
            });
            
            _platform = std::shared_ptr<impl_t>(ptr);

#if defined(__EMSCRIPTEN__)
            file::Path path("./fonts/Quicksand.ttf");
            font_data = path.retrieve_data();
#endif
            init(title);
        }
        
        void set_graph(DrawStructure& base) {
            _graph = &base;
            
        }
        void init(const std::string& title, bool soft = false);
        ~IMGUIBase();
        
        void set_open_files_fn(std::function<bool(const std::vector<file::Path>&)> fn) {
            _open_files_fn = fn;
        }
        
        void set_background_color(const Color&) override;
        void set_frame_recording(bool v) override;
        const Image::UPtr& current_frame_buffer() override;
        void loop();
        LoopStatus update_loop() override;
        virtual void paint(DrawStructure& s) override;
        void set_title(std::string) override;
        const std::string& title() const override { return _title; }
        
        Bounds text_bounds(const std::string& text, Drawable*, const Font& font) override;
        uint32_t line_spacing(const Font& font) override;
        Size2 window_dimensions() override;
        Size2 real_dimensions();
        template<class F, class... Args>
        auto exec_main_queue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>
        {
            std::lock_guard<std::mutex> guard(_mutex);
            using return_type = typename std::invoke_result_t<F, Args...>;
            auto task = std::make_shared< std::packaged_task<return_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            
            auto future = task->get_future();
            _exec_main_queue.push([task](){ (*task)(); });
            //_exec_main_queue.push(std::bind([](F& fn){ fn(); }, std::move(fn)));
            return future;
        }
        Event toggle_fullscreen(DrawStructure& g) override;
        
    private:
        void redraw(Drawable* o, std::vector<DrawOrder>& draw_order, bool is_background = false, ImVec4 clip_rect = ImVec4());
        void draw_element(const DrawOrder& order);
        void event(const gui::Event& e);
        static void update_size_scale(GLFWwindow*);
    };
}
