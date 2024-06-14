#pragma once
#if __APPLE__ && __has_include(<Metal/Metal.h>)
#define COMMONS_METAL_AVAILABLE true

#include <gui/CrossPlatform.h>
#include <misc/Timer.h>
struct GLFWwindow;

namespace cmn::gui {
    struct MetalData;

    class MetalImpl : public CrossPlatform {
        GLFWwindow *window = nullptr;
        std::function<void()> draw_function;
        std::function<bool()> new_frame_fn;
        MetalData* _data;
        
        double _draw_calls = 0;
        Timer _draw_timer;
        
        Image::Ptr _current_framebuffer;
        std::mutex _texture_mutex;
        std::vector<void*> _delete_textures;
        
        std::atomic<size_t> frame_index;
        std::thread::id _update_thread;
        CallbackCollection _callback;
        
        std::function<void()> _after_frame;
        std::function<void(Image::Ptr&&)> _frame_buffer_receiver;
        GETTER_I(bool, gui_macos_blur, false);
        
    public:
        MetalImpl(std::function<void()> draw, std::function<bool()> new_frame_fn);
        float center[2] = {0.5f, 0.5f};
        
        void init() override;
        void post_init() override;
        void create_window(const char* title, int width, int height) override;
        LoopStatus update_loop(const custom_function_t& = nullptr) override;
        TexturePtr texture(const Image*) override;
        void clear_texture(TexturePtr&&) override;
        void bind_texture(const PlatformTexture&) override;
        void update_texture(PlatformTexture&, const Image*) override;
        void set_title(std::string) override;
        void toggle_full_screen() override;
        void set_frame_buffer_receiver(std::function<void(Image::Ptr&&)> fn) override;
        void message(const std::string&) const;
        
        virtual ~MetalImpl();
        GLFWwindow* window_handle() override;
    public:
        bool open_files(const std::vector<file::Path>&);
        void check_thread_id(int line, const char* file) const;
    };
}

#else
#define COMMONS_METAL_AVAILABLE false
#endif
