#pragma once

#include <commons.pc.h>
#include "CrossPlatform.h"
#include <gui/DrawBase.h>
#include <misc/Timer.h>
#include <gui/Transform.h>
#ifndef NDEBUG
#include <gui/types/Drawable.h>
#endif

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
using default_impl_t = cmn::gui::MetalImpl;
#else
using default_impl_t = cmn::gui::GLImpl;
#endif

namespace cmn::gui {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
    ENUM_CLASS(Effects, blur)
#ifdef __clang__
#pragma clang diagnostic pop
#endif

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
        
        std::once_flag _resize_init_flag;
        GETTER(std::unique_ptr<DrawStructure>, graph);
        GETTER_NCONST(std::shared_ptr<CrossPlatform>, platform);
        CrossPlatform::custom_function_t _custom_loop;
        GETTER(Bounds, work_area);
        GETTER_I(bool, focussed, true);
        std::function<void(DrawStructure&, const gui::Event&)> _event_fn;
        size_t _objects_drawn, _skipped;
#ifndef NDEBUG
        std::unordered_map<Type::Class, size_t> _type_counts;
#endif
        Timer _last_debug_print;
        Size2 _last_framebuffer_size;
        GETTER(Float2_t, dpi_scale);
        Float2_t _last_dpi_scale = -1;
        std::string _title;
        std::function<bool(const std::vector<file::Path>&)> _open_files_fn;
        
    public:
        struct DrawOrder {
            enum Type {
                DEFAULT = 0,
                BACKGROUND,
                BACKGROUND_LINE,
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
        
    protected:
        std::unordered_map<Drawable*, std::tuple<int, Vec2>> _rotation_starts;
        std::vector<DrawOrder> _draw_order, _above_z;
        
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
        
        void draw_loop();
        bool new_frame_function();
        
    public:
        template<typename impl_t = default_impl_t, typename Graph = Size2>
        IMGUIBase(std::string title,
                  Graph&& window,
                  std::function<bool(DrawStructure&)> custom_loop,
                  std::function<void(DrawStructure&, const gui::Event&)> event_fn)
        : _event_fn(event_fn), _title(title)
        {
            if constexpr(are_the_same<Graph, Size2>)
                _graph = std::make_unique<DrawStructure>(window.width, window.height);
            else if constexpr(are_the_same<Graph, std::unique_ptr<DrawStructure>>) {
                _graph = std::move(window);
            } else {
                static_assert(are_the_same<void, Graph>, "Invalid type of Graph.");
            }
            
            _custom_loop = [this, custom_loop = std::move(custom_loop) ](){
                return custom_loop(*_graph);
            };
            
            auto ptr = new impl_t([this](){
                draw_loop();
            }, [this]() -> bool {
                return new_frame_function();
            });
            
            _platform = std::shared_ptr<impl_t>(ptr);

#if defined(__EMSCRIPTEN__)
            file::Path path("./fonts/Quicksand.ttf");
            font_data = path.retrieve_data();
#endif
            init(title);
        }
        
        /*void set_graph(std::unique_ptr<DrawStructure>&& base) {
            _graph = std::move(base);
        }*/
        void init(const std::string& title, bool soft = false);
        void center(const Size2&);
        ~IMGUIBase();
        
        void set_open_files_fn(std::function<bool(const std::vector<file::Path>&)> fn) {
            _open_files_fn = fn;
        }
        
        Float2_t get_scale_multiplier();
        void set_background_color(const Color&) override;
        void set_frame_recording(bool v) override;
        void loop();
        LoopStatus update_loop() override;
        virtual void paint(DrawStructure& s) override;
        void set_title(std::string) override;
        const std::string& title() const override { return _title; }
        
        Bounds text_bounds(const std::string& text, Drawable*, const Font& font) override;
        Float2_t line_spacing(const Font& font) override;

        Size2 window_dimensions() const override;
        void set_window_size(Size2) override;
        void set_window_bounds(Bounds) override;
        Bounds get_window_bounds() const override;

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
        void process_main_queue();
        Event toggle_fullscreen(DrawStructure& g) override;
        
    private:
        void redraw(Drawable* o, std::vector<DrawOrder>& draw_order, std::vector<DrawOrder>& above_z, bool is_background = false, ImVec4 clip_rect = ImVec4());
        void draw_element(const DrawOrder& order);
        void event(const gui::Event& e);
        static void update_size_scale(GLFWwindow*);
        void set_last_framebuffer(Size2);
        void draw_debug_rectangle(Drawable* o);
    };
}
