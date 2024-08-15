#include "IMGUIBase.h"
#include <gui/DrawStructure.h>

#include <algorithm>
#include <limits>

#include "MetalImpl.h"
#include "GLImpl.h"

#include <imgui.h>
#include <imgui_internal.h>

#define IM_NORMALIZE2F_OVER_ZERO(VX,VY)     { Float2_t d2 = VX*VX + VY*VY; if (d2 > 0_F) { Float2_t inv_len = 1_F / ImSqrt(d2); VX *= inv_len; VY *= inv_len; } }
#define IM_FIXNORMAL2F(VX,VY)               { Float2_t d2 = VX*VX + VY*VY; if (d2 < 0.5_F) d2 = 0.5_F; Float2_t inv_lensq = 1_F / d2; VX *= inv_lensq; VY *= inv_lensq; }

#define GLFW_INCLUDE_GL3  /* don't drag in legacy GL headers. */
#define GLFW_NO_GLU       /* don't drag in the old GLU lib - unless you must. */

#include <GLFW/glfw3.h>

#include <misc/GlobalSettings.h>
#include <file/DataLocation.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#endif

#if GLFW_VERSION_MAJOR <= 3 && GLFW_VERSION_MINOR <= 2
#define GLFW_HAVE_MONITOR_SCALE false
#if !defined(__EMSCRIPTEN__)
#include <GLFW/glfw3native.h>
#endif
#else
#define GLFW_HAVE_MONITOR_SCALE true
#endif

#include <misc/colors.h>

namespace cmn::gui {


size_t cache_misses = 0, cache_finds = 0;

void cache_miss() {
    ++cache_misses;
}

void cache_find() {
    ++cache_finds;
}

void clear_cache() {
    cache_finds = cache_misses = 0;
}

class PolyCache : public CacheObject {
    GETTER_NCONST(std::vector<ImVec2>, points);
};

#ifndef NDEBUG
    class TextureCache;
    std::set<ImTextureID> all_gpu_texture;
#endif

    class TextureCache : public CacheObject {
        GETTER(TexturePtr, texture);
        GETTER_NCONST(Size2, size);
        GETTER_NCONST(Size2, gpu_size);
        GETTER_PTR(IMGUIBase*, base);
        GETTER_PTR(CrossPlatform*, platform);
        GETTER_PTR(Drawable*, object);
        GETTER_SETTER(uint32_t, channels);
        
        //static std::unordered_map<const IMGUIBase*, std::set<const TextureCache*>> _all_textures;
        //static std::mutex _texture_mutex;
        
    public:
        static std::unique_ptr<TextureCache> get(IMGUIBase* base) {
            auto ptr = std::make_unique<TextureCache>();
            ptr->set_base(base);
            ptr->platform() = base->platform().get();
            
            //std::lock_guard<std::mutex> guard(_texture_mutex);
            //_all_textures[base].insert(ptr.get());
            
            return ptr;
        }
        
        static void remove_base(IMGUIBase* ) {
            /*std::unique_lock<std::mutex> guard(_texture_mutex);
            for(auto & tex : _all_textures[base]) {
                tex->set_ptr(nullptr);
                tex->set_base(nullptr);
                tex->platform() = nullptr;
            }
            _all_textures.erase(base);*/
        }
        
    public:
        TextureCache()
            : _texture(nullptr), _base(nullptr), _platform(nullptr), _object(nullptr), _channels(0)
        {
            
        }
        
        TextureCache(TexturePtr&& ptr)
            : _texture(std::move(ptr)), _base(nullptr), _platform(nullptr), _object(nullptr), _channels(0)
        {
            
        }
        
    public:
        void set_base(IMGUIBase* base) {
            _base = base;
        }
        
        void set_ptr(TexturePtr&& ptr) {
            /*if(_texture) {
                if(_platform) {
                    assert(_base);
                    _base->exec_main_queue([ptr = std::move(_texture), cache = this, base = _platform]() mutable
                    {
                        if(ptr) {
#ifndef NDEBUG
                            std::lock_guard<std::mutex> guard(_texture_mutex);
                            auto it = all_gpu_texture.find(ptr->ptr);
                            if(it != all_gpu_texture.end()) {
                                all_gpu_texture.erase(it);
                            } else
                                Print("Cannot find GPU texture of ",cache);
#endif
                            ptr = nullptr;
                            //base->clear_texture(std::move(ptr));
                        }
                    });
                } else
                    FormatExcept("Cannot clear texture because platform is gone.");
            }*/
            
#ifndef NDEBUG
            if(_texture) {
                auto it = all_gpu_texture.find((ImTextureID)_texture->ptr);
                if(it != all_gpu_texture.end()) {
                    all_gpu_texture.erase(it);
                } else
                    Print("Cannot find GPU texture of ",this);
            }
#endif
            
            _texture = std::move(ptr);
            
#ifndef NDEBUG
            if(_texture) {
                all_gpu_texture.insert((ImTextureID)_texture->ptr);
            }
#endif
        }
        
        static Size2 gpu_size_of(const ExternalImage* image) {
            if(!image || !image->source())
                return Size2();
#if defined(__EMSCRIPTEN__)
            return image->source()->bounds().size();
#else
            return Size2(next_pow2(sign_cast<uint64_t>(image->source()->bounds().width)),
                         next_pow2(sign_cast<uint64_t>(image->source()->bounds().height)));
#endif
        }
        
        void update_with(const ExternalImage* image) {
            auto image_size = image->source()->bounds().size();
            auto image_channels = image->source()->dims;
            auto gpusize = gpu_size_of(image);
            //static size_t replacements = 0, adds = 0, created = 0;
            if(!_texture) {
                auto id = _platform->texture(image->source());
                set_ptr(std::move(id));
                _size = image_size;
                _channels = image_channels;
                //++created;
                
            } else if(gpusize.width > _texture->width
                      || gpusize.height > _texture->height
                      || gpusize.width < _texture->width/4
                      || gpusize.height < _texture->height/4
                      || channels() != image_channels)
            {
                auto id = _platform->texture(image->source());
                set_ptr(std::move(id));
                _size = image_size;
                _channels = image_channels;
                //++adds;
                
            } else if(changed()) {
                _platform->update_texture(*_texture, image->source());
                set_changed(false);
                //++replacements;
            }
            
            //if(replacements%100 == 0)
        }
        
        void set_object(Drawable* o) {
            auto pobj = _object;
            if(pobj && pobj != o) {
                _object = o;
                pobj->remove_cache(_base);
                if(o)
                    FormatWarning("Exchanging cache for object");
            }
        }
        
        ~TextureCache() {
            set_ptr(nullptr);
            
            /*if(_base ) {
                std::lock_guard<std::mutex> guard(_texture_mutex);
                auto &tex = _all_textures[_base];
                for(auto it=tex.begin(); it != tex.end(); ++it) {
                    if(it->get() == this) {
                        tex.erase(it);
                        break;
                    }
                }
            }*/
        }
    };

    //std::unordered_map<const IMGUIBase*, std::set<std::shared_ptr<TextureCache>>> TextureCache::_all_textures;
    //std::mutex TextureCache::_texture_mutex;

    constexpr Codes glfw_key_map[GLFW_KEY_LAST + 1] {
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Space,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, // apostroph (39)
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Comma, // 44
        Codes::Subtract,
        Codes::Period,
        Codes::Slash,
        Codes::Num0, Codes::Num1, Codes::Num2, Codes::Num3, Codes::Num4, Codes::Num5, Codes::Num6, Codes::Num7, Codes::Num8, Codes::Num9,
        Codes::Unknown,
        Codes::SemiColon, //(69),
        Codes::Unknown,
        Codes::Equal, // (61)
        Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::A, Codes::B, Codes::C, Codes::D, Codes::E, Codes::F, Codes::G, Codes::H, Codes::I, Codes::J, Codes::K, Codes::L, Codes::M, Codes::N, Codes::O, Codes::P, Codes::Q, Codes::R, Codes::S, Codes::T, Codes::U, Codes::V, Codes::W, Codes::X, Codes::Z, Codes::Y,
        Codes::LBracket,
        Codes::BackSlash,
        Codes::RBracket, // (93)
        Codes::Unknown, Codes::Unknown,
        Codes::Unknown, // (grave accent, 96)
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, // world 1 (161)
        Codes::Unknown, // world 2 (162)
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, // 255
        
        Codes::Escape,
        Codes::Return,
        Codes::Tab,
        Codes::BackSpace,
        Codes::Insert,
        Codes::Delete,
        Codes::Right,
        Codes::Left,
        Codes::Down,
        Codes::Up,
        Codes::PageUp,
        Codes::PageDown,
        Codes::Home,
        Codes::End, // 269
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        Codes::Unknown, // capslock (280)
        Codes::Unknown, // scroll lock (281)
        Codes::Unknown, // num_lock (282)
        Codes::Unknown, // print screen (283)
        Codes::Pause, // 284
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, 
        // 290
        Codes::F1, Codes::F2, Codes::F3, Codes::F4, Codes::F5, Codes::F6, Codes::F7, Codes::F8, Codes::F9, Codes::F10, Codes::F11, Codes::F12, Codes::F13, Codes::F14, Codes::F15, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, // 314
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::Numpad0, Codes::Numpad1, Codes::Numpad2, Codes::Numpad3, Codes::Numpad4, Codes::Numpad5, Codes::Numpad6, Codes::Numpad7, Codes::Numpad8, Codes::Numpad9, // 329
        
        // numpad
        Codes::Comma,
        Codes::Divide,
        Codes::Multiply,
        Codes::Subtract,
        Codes::Add,
        Codes::Return,
        Codes::Equal, // 336
        
        Codes::Unknown, Codes::Unknown, Codes::Unknown,
        
        Codes::LShift, // 340
        Codes::LControl,
        Codes::LAlt,
        Codes::LSystem,
        Codes::RShift,
        Codes::RControl,
        Codes::RAlt,
        Codes::RSystem,
        Codes::Menu // 348
    };

    /*struct CachedFont {
        ImFont *ptr;
        float base_size;
        float line_spacing;
    };*/

    std::unordered_map<uint32_t, ImFont*> _fonts;
    Float2_t im_font_scale = 2;

    std::unordered_map<GLFWwindow*, IMGUIBase*> base_pointers;

#if defined(__EMSCRIPTEN__)
/*void IMGUIBase::downloadSuccess(emscripten_fetch_t* fetch) {
    printf("Finished downloading %llu bytes from URL %s.\n", fetch->numBytes, fetch->url);
    font_data.resize(fetch->numBytes);
    std::copy(fetch->data, fetch->data + fetch->numBytes, font_data.data());
    // The data is now available at fetch->data[0] through fetch->data[fetch->numBytes-1];

    auto self = (IMGUIBase*)fetch->userData;
    emscripten_fetch_close(fetch); // Free data associated with the fetch.
    //self->init(self->title());
    self->_download_finished = true;
    self->_download_variable.notify_all();
}

void IMGUIBase::downloadFailed(emscripten_fetch_t* fetch) {
    printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
    auto self = (IMGUIBase*)fetch->userData;
    emscripten_fetch_close(fetch); // Also free data on failure.
    //self->init(self->title());
    self->_download_finished = true;
    self->_download_variable.notify_all();
}*/


EM_JS(int, canvas_get_width, (), {
    return canvas.width;
});

EM_JS(int, canvas_get_height, (), {
    return canvas.height;
});

/*EM_JS(void, canvas_set_, (int w, int h), {
    canvas.width = w;
    canvas.height = h;
});*/

void canvas_set_(int w, int h) {
    /*auto canvas_css_resize_result = emscripten_set_element_css_size(
        "canvas",
        w,
        h
    );*/
}
/*EM_JS(void, canvas_set, (int w, int h), {

    //canvas.width = w;
    //canvas.height = h;
});*/
#endif

void set_window_size(GLFWwindow* window, Size2 size) {
#if defined(__EMSCRIPTEN__)
    glfwSetWindowSize(window, size.width, size.height);
    canvas_set_(size.width, size.height);
#else
    glfwSetWindowSize(window, size.width, size.height);
#endif
}

Float2_t IMGUIBase::get_scale_multiplier() {
#if defined(__EMSCRIPTEN__)
    return 1.f;//emscripten_get_device_pixel_ratio();
#else
    return 1.0 / _dpi_scale;
#endif
}

Size2 get_window_size(GLFWwindow* window, Float2_t ) {
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    //w *= r;
    //h *= r;
    return { Float2_t(w), Float2_t(h) };
}

Size2 get_frame_buffer_size(GLFWwindow* window, Float2_t r) {
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    display_w *= r;
    display_h *= r;
    return { Float2_t(display_w), Float2_t(display_h) };
}

void IMGUIBase::set_window_size(Size2 size) {
    ::cmn::gui::set_window_size(_platform->window_handle(), size);
}

GLFWmonitor* get_monitor_for(GLFWwindow* window) {
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    auto ws = get_window_size(window, 1);
    
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if(not monitor) {
        int count;
        auto monitors = glfwGetMonitors(&count);
        int mx, my, mw, mh;
#if !defined(__EMSCRIPTEN__)
        for (int i=0; i<count; ++i) {
            if(not monitors[i])
                continue;
            
            glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
#ifndef NDEBUG
            auto name = glfwGetMonitorName(monitors[i]);
            Print("Monitor ",name,": ",mx,",",my," ",mw,"x",mh);
#endif
            if(Bounds(mx+5, my+5, mw-10, mh-10).overlaps(Bounds(x+5, y+5, ws.width-10, ws.height-10))) {
                monitor = monitors[i];
                break;
            }
        }
        
        if(not monitor) {
            // assume fullscreen?
            Print("No monitor found.");
        }
#endif
    }
    
    return monitor;
}

void IMGUIBase::set_window_bounds(Bounds bounds) {
    auto size = bounds.size();
    auto pos = bounds.pos();
    auto monitor = get_monitor_for(_platform->window_handle());
    if(monitor) {
        int mx, my, mw, mh;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        if(my % 500 < 100 && my % 500 > -100)
            my -= my % 500;
        
        pos += Vec2(mx, my);
    }
	glfwSetWindowPos(_platform->window_handle(), pos.x, pos.y);
    
    if(size.width <= 160)
        size.width = 160;
    if(size.height <= 160)
        size.height = 160;
	this->set_window_size(size);
    this->update_size_scale(_platform->window_handle());
}

Bounds IMGUIBase::get_window_bounds() const {
    int x, y;
    glfwGetWindowPos(_platform->window_handle(), &x, &y);
    auto size = get_window_size(_platform->window_handle(), 1);
    return Bounds(x, y, size.width, size.height);
}

Size2 frame_buffer_scale(GLFWwindow* window, Float2_t r) {
    // Setup display size (every frame to accommodate for window resizing)
    auto window_size = get_window_size(window, 1);
    auto fb = get_frame_buffer_size(window, r);

    Size2 DisplayFramebufferScale(1,1);
    if (window_size.width > 0 && window_size.height > 0)
        DisplayFramebufferScale = fb.div(window_size);

    //Print("Size: ", DisplaySize, " Scale: ", DisplayFramebufferScale);
    return window_size;
}

Bounds get_work_area(GLFWmonitor* monitor) {
    int mx, my, mw, mh;
    mx = my = 0;
    mw = 640;
    mh = 480;
    
    if(monitor) {
#if GLFW_HAVE_MONITOR_SCALE
#if !defined(__APPLE__)
        auto video_mode = glfwGetVideoMode(monitor);
        if (video_mode) {
            mw = video_mode->width;
            mh = video_mode->height;
        } else
#endif
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
#else
        auto video_mode = glfwGetVideoMode(monitor);
        mx = my = 0;
        if (video_mode) {
            mw = video_mode->width;
            mh = video_mode->height;
        }
#endif
    } else {
        FormatWarning("[GLFW] Cannot get monitor for window.");
    }
    return Bounds(mx, my, mw, mh);
}

void IMGUIBase::center(const Size2 &size) {
    auto work_area = this->work_area();
    auto window_size = size.empty()
        ? Size2(work_area.width * 0.75, work_area.width * 0.75 * 0.7)
        : size;
    auto max_height = work_area.height * 0.95;
    if(window_size.height > max_height) {
        auto ratio = window_size.width / window_size.height;
        window_size = Size2(max_height * ratio, max_height);
    }
    
    Bounds bounds(
        Vec2(),
        window_size);
    
    int offset = (work_area.height - max_height);
#if !defined(__APPLE__)
    offset = 0;
#endif
    Print("Calculated bounds = ", bounds, " from window size = ", window_size, " and work area = ", work_area, " with offset = ", offset);
    bounds.restrict_to(Bounds(work_area.size()));
    bounds << Vec2(work_area.width / 2 - bounds.width / 2,
                    work_area.height / 2 - bounds.height / 2 + offset);
    bounds.restrict_to(Bounds(work_area.size()));
    Print("Restricting bounds to work area: ", work_area, " -> ", bounds);

    Print("setting bounds = ", bounds);
    //window()->set_window_size(window_size);
    set_window_bounds(bounds);
}

void IMGUIBase::update_size_scale(GLFWwindow* window) {
    auto base = base_pointers.at(window);
    //auto lock_guard = GUI_LOCK(base->_graph->lock());
    
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    
    GLFWmonitor* monitor = get_monitor_for(window);
    if(not monitor)
        return; // we cannot continue off screen
    
    base->_work_area = get_work_area(monitor);
    
    float xscale, yscale;
#if GLFW_HAVE_MONITOR_SCALE
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
#else
    xscale = yscale = emscripten_get_device_pixel_ratio();
#endif

#ifndef NDEBUG
    Print("Content scale: ", xscale,"x",yscale, " monitor = ", base->_work_area);
#endif
    
    Float2_t dpi_scale = 1 / Float2_t(max(xscale, yscale));//max(float(fw) / float(width), float(fh) / float(height));
    //auto& io = ImGui::GetIO();

#if defined(__EMSCRIPTEN__)
    dpi_scale = 0.5; // emscripten_get_device_pixel_ratio();
    //io.DisplayFramebufferScale = ImVec2(emscripten_get_device_pixel_ratio(), emscripten_get_device_pixel_ratio());
#endif
    //SETTING(gui_interface_scale) = Float2_t(1);// / dpi_scale);
    im_font_scale = max(1, dpi_scale) * 0.75_F;
    base->_dpi_scale = dpi_scale;

    const Float2_t interface_scale = gui::interface_scale();
    base->_graph->set_scale(1.0_F / interface_scale);

    int fw, fh;
#if __APPLE__
    auto fb = frame_buffer_scale(window, 1);
#else
    auto r = base->get_scale_multiplier();
    auto fb = frame_buffer_scale(window, r);
#endif
    fw = fb.width;
    fh = fb.height;

    //base->set_last_framebuffer(Size2(fw, fh));

    //Print("Framebuffer scale: ", fw, "x", fh, "@", base->_dpi_scale, " graph scale: ", base->_graph->scale());
    
    {
        base->_graph->set_size(Size2(fw, fh).mul(base->_dpi_scale));

        //int ww, wh;
        //glfwGetWindowSize(window, &ww, &wh);
        //Print("Window size: ", ww, "x", wh, " -> ", fw, "x", fh, " previous:", base->_last_dpi_scale);
        
        Event e(EventType::WINDOW_RESIZED);
#if __APPLE__
        e.size.width = fw;
        e.size.height = fh;
#else
        e.size.width = fw * base->dpi_scale();
        e.size.height = fh * base->dpi_scale();
#endif

        base->event(e);
    }
    
    base->_graph->set_dirty(NULL);

#if WIN32
    if (base->_last_dpi_scale != -1 && base->_last_dpi_scale != dpi_scale && dpi_scale > 0) {
        auto p = base->_last_dpi_scale / dpi_scale;
        base->_last_dpi_scale = dpi_scale;
        ::cmn::gui::set_window_size(window, { fw * p, fh * p });
        //glfwSetWindowSize(window, fw * p, fh * p);
    }
    else {
        base->_last_dpi_scale = dpi_scale;
    }
#endif
}

    ImU32 cvtClr(const gui::Color& clr) {
        return ImColor(clr.r, clr.g, clr.b, clr.a);
    }

    void IMGUIBase::init(const std::string& title, bool soft) {
        _platform->init();
        
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        float xscale, yscale;
#if GLFW_HAVE_MONITOR_SCALE
        glfwGetMonitorContentScale(monitor, &xscale, &yscale);
#else
        xscale = yscale = emscripten_get_device_pixel_ratio();
#endif
        int width = _graph->width(), height = _graph->height();
#if defined(__EMSCRIPTEN__)
        width = canvas_get_width();
        height = canvas_get_height();
        Print("Canvas size: ", width, " ", height);
#endif

#ifdef WIN32
        width *= xscale;
        height *= yscale;
#endif
        _work_area = get_work_area(monitor);
        
        if(!_platform->window_handle())
#ifdef WIN32
            _platform->create_window(title.c_str(), 1, 1);
#else
            _platform->create_window(title.c_str(), width, height);
#endif
        else {
            set_title(title);
        }
        
        base_pointers[_platform->window_handle()] = this;
        center({});

        glfwSetDropCallback(_platform->window_handle(), [](GLFWwindow* window, int N, const char** texts){
            std::vector<file::Path> _paths;
            for(int i=0; i<N; ++i)
                _paths.push_back(texts[i]);
            if(base_pointers.count(window)) {
                if(base_pointers[window]->_open_files_fn) {
                    base_pointers[window]->_open_files_fn(_paths);
                }
            }
        });
        _platform->set_open_files_fn([this](auto& files) -> bool {
            if(_open_files_fn)
                return _open_files_fn(files);
            return false;
        });

        file::Path path("fonts/Quicksand");
        if(file::DataLocation::is_registered("app")) {
            path = file::DataLocation::parse("app", path);
        }
        if (!path.add_extension("ttf").exists())
            FormatExcept("Cannot find file ",path.str(),"");
        
        auto& io = ImGui::GetIO();
        //io.FontAllowUserScaling = true;
        //io.WantCaptureMouse = false;
        //io.WantCaptureKeyboard = false;
        
        
        const Float2_t base_scale = 32;
        //float dpi_scale = max(float(fw) / float(width), float(fh) / float(height));
        Float2_t dpi_scale = 1 / Float2_t(max(xscale, yscale));
#if defined(__EMSCRIPTEN__)
        //Print("dpi:",emscripten_get_device_pixel_ratio());
        dpi_scale = 0.5;
        //io.DisplayFramebufferScale = { dpi_scale, dpi_scale };
        //io.FontGlobalScale = 1.f / dpi_scale;
#endif
        im_font_scale = max(1, dpi_scale) * 0.75_F;
        _dpi_scale = dpi_scale;
        
        int fw, fh;
        //glfwGetFramebufferSize(_platform->window_handle(), &fw, &fh);
        //auto fb = get_frame_buffer_size(_platform->window_handle(), get_scale_multiplier());

        auto fb = get_frame_buffer_size(_platform->window_handle(), 1);
        fw = fb.width;
        fh = fb.height;
        set_last_framebuffer(Size2(fw, fh));
        //_last_framebuffer_size = Size2(fw, fh);//.mul(_dpi_scale);
        
        if (!soft) {
            io.Fonts->Clear();
            _fonts.clear();
        }

        if(_fonts.empty()) {
            ImFontConfig config;
            config.OversampleH = 5;
            config.OversampleV = 5;

            auto load_font = [&](int no, std::string suffix, const file::Path& path, std::string = ".ttf", Float2_t scale = 1_F, bool = false) {
                config.FontNo = no;
                if (no > 0)
                    config.MergeMode = false;
                auto full = path.str() + suffix + ".ttf";
#if defined(__EMSCRIPTEN__)
                Print("Loading font...");
                auto ptr = io.Fonts->AddFontFromMemoryTTF((void*)font_data.data(), font_data.size() * sizeof(decltype(font_data)::value_type), base_scale * im_font_scale * scale, &config);
#else
                static const ImWchar all_ranges[] = {
                    // Range from Basic Latin to Cyrillic Supplement
                    0x0020, 0x00FF,  // ö
                    // Range from Cyrillic + Cyrillic Supplement
                    0x0400, 0x052F,

                    // Range from General Punctuation to Arrows
                    0x2000, 0x206F, // General Punctuation
                    0x2070, 0x209F, // Superscripts and Subscripts
                    0x20A0, 0x20CF, // Currency Symbols
                    0x2190, 0x27BF, // Arrows

                    // Range from Miscellaneous Technical to Miscellaneous Symbols and Play Symbol
                    //0x2300, 0x26FF, // Miscellaneous Technical
                    //0x2600, 0x26FF, // Miscellaneous Symbols
                    //0x2700, 0x27BF, // Dingbats
                    //0x25B6, 0x25B6, // Play Symbol
                    0x2B00, 0x2EFF,

                    // CJK Symbols and Punctuation to Hiragana and Katakana
                    0x3000, 0x303F, // CJK Symbols and Punctuation
                    0x3040, 0x309F, // Hiragana
                    0x30A0, 0x30FF, // Katakana

                    // CJK Unified Ideographs Extension A
                    0x3400, 0x4DBF,

                    // CJK Unified Ideographs
                    0x4E00, 0x9FFF,
                    
                    0x1F300, 0x1F900, // Emoticons (U+1F600 to U+1F64F)

                    // Sentinel to indicate end of ranges
                    0x0
                };

                auto ptr = io.Fonts->AddFontFromFileTTF(full.c_str(), base_scale * im_font_scale * scale, &config, all_ranges); //add_all_ranges ? all_ranges : io.Fonts->GetGlyphRangesCyrillic());
#endif
                if (!ptr) {
                    FormatWarning("Cannot load font ", path.str()," with index ",config.FontNo,". {CWD=", file::cwd(),"}");
                    ptr = io.Fonts->AddFontDefault();
                    im_font_scale = max(1, dpi_scale) * 0.5_F;
                }
                //ptr->ConfigData->GlyphOffset = ImVec2(1,0);
                ptr->FontSize = base_scale * im_font_scale * scale;

                return ptr;
            };

            _fonts[Style::Regular] = load_font(0, "", path);
            _fonts[Style::Italic] = load_font(0, "-i", path);
            _fonts[Style::Bold] = load_font(0, "-b", path);
            _fonts[Style::Bold | Style::Italic] = load_font(0, "-bi", path);
            
            file::Path mono("fonts/PTMono");
            if(file::DataLocation::is_registered("app")) {
                mono = file::DataLocation::parse("app", mono);
            }
            if (not mono.add_extension("ttf").exists())
                FormatExcept("Cannot find file ",mono.str());
            
            file::Path syms("fonts/NotoSansSymbols2-Regular");
            if(file::DataLocation::is_registered("app")) {
                syms = file::DataLocation::parse("app", syms);
            }
            if (not syms.add_extension("ttf").exists())
                FormatExcept("Cannot find file ",syms.str());
            
            config.GlyphOffset.y = 1.8;
            config.GlyphOffset.x = -1;
            _fonts[Style::Monospace] = load_font(0, "", mono, ".ttf", 0.85);
            _fonts[Style::Monospace | Style::Italic] = load_font(0, "", mono, ".ttf", 0.85);
            _fonts[Style::Monospace | Style::Bold] = load_font(0, "-Bold", mono, ".ttf", 0.85, true);
            
            config.GlyphOffset.y = 3;
            config.GlyphOffset.x = 0;
            _fonts[Style::Symbols] = load_font(0, "", syms, ".ttf", 1, true);
            
            config.GlyphOffset.x = 0;
            config.GlyphOffset.y = 0;

            io.Fonts->Build();
        }

        _platform->post_init();
        _platform->set_title(title);
        
        _focussed = true;
        
        glfwSetWindowFocusCallback(_platform->window_handle(), [](GLFWwindow* window, int focus) {
            auto base = base_pointers.at(window);
            base->_focussed = focus == GLFW_TRUE;
        });
        glfwSetKeyCallback(_platform->window_handle(), [](GLFWwindow* window, int key, int , int action, int) {
            auto base = base_pointers.at(window);
            
            Event e(EventType::KEY);
            e.key.pressed = action == GLFW_PRESS || action == GLFW_REPEAT;
            assert(key <= GLFW_KEY_LAST);
            e.key.code = glfw_key_map[key];
            e.key.shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            
            base->event(e);
            base->_graph->set_dirty(NULL);
        });
        glfwSetCursorPosCallback(_platform->window_handle(), [](GLFWwindow* window, double xpos, double ypos) {
            auto base = base_pointers.at(window);
            //if (not base->_focussed)
            //    return;
            
            Event e(EventType::MMOVE);
            auto &io = ImGui::GetIO();
            e.move.x = Float2_t(xpos * io.DisplayFramebufferScale.x) * base->_dpi_scale;
            e.move.y = Float2_t(ypos * io.DisplayFramebufferScale.y) * base->_dpi_scale;
            
            base->event(e);
            
            base->_graph->set_dirty(NULL);
            /**/
        });
        glfwSetMouseButtonCallback(_platform->window_handle(), [](GLFWwindow* window, int button, int action, int) {
            if(button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT)
                return;
            
            Event e(EventType::MBUTTON);
            e.mbutton.pressed = action == GLFW_PRESS;
            
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            auto base = base_pointers.at(window);
            auto &io = ImGui::GetIO();
            e.mbutton.x = Float2_t(xpos * io.DisplayFramebufferScale.x) * base->_dpi_scale;
            e.mbutton.y = Float2_t(ypos * io.DisplayFramebufferScale.y) * base->_dpi_scale;
            e.mbutton.button = GLFW_MOUSE_BUTTON_RIGHT == button ? 1 : 0;

            base->event(e);
            base->_graph->set_dirty(NULL);
        });
        glfwSetScrollCallback(_platform->window_handle(), [](GLFWwindow* window, double xoff, double yoff) {
            Event e(EventType::SCROLL);
            e.scroll.dy = Float2_t(yoff);
            e.scroll.dx = Float2_t(xoff);
            
            auto base = base_pointers.at(window);
            base->event(e);
            base->_graph->set_dirty(NULL);
        });
        glfwSetCharCallback(_platform->window_handle(), [](GLFWwindow* window, unsigned int c) {
            Event e(EventType::TEXT_ENTERED);
            e.text.c = char(c);
            
            auto base = base_pointers.at(window);
            base->event(e);
            base->_graph->set_dirty(NULL);
        });
        glfwSetWindowSizeCallback(_platform->window_handle(), [](GLFWwindow* window, int width, int height)
        {
            //Print("Updating size callback: ", width, "x", height);
                //width /= 2;
                //height /= 2;
            static int prev_width = 0, prev_height = 0;
            if (prev_width == width && prev_height == height)
                return;
            prev_width = width;
            prev_height = height;
            IMGUIBase::update_size_scale(window);
        });
        
        glfwSetWindowPosCallback(_platform->window_handle(), [](GLFWwindow* window, int, int)
        {
            IMGUIBase::update_size_scale(window);
        });
        
        exec_main_queue([window = _platform->window_handle()](){
            IMGUIBase::update_size_scale(window);
        });

        Print("IMGUIBase::init complete");
    }

void IMGUIBase::process_main_queue() {
    std::unique_lock guard(_mutex);
    while(!_exec_main_queue.empty()) {
        auto p = std::move(_exec_main_queue.front());
        guard.unlock();
        try {
            p();
        } catch(const std::exception& ex) {
            FormatExcept("Exception in main queue: ", ex.what());
            throw;
        }
        guard.lock();
        _exec_main_queue.pop();
    }
}

    IMGUIBase::~IMGUIBase() {
        while(!_exec_main_queue.empty()) {
            (_exec_main_queue.front())();
            _exec_main_queue.pop();
        }
        
        /*std::queue<SectionInterface*> q;
        q.push((SectionInterface*)&_graph->root());
        while (!q.empty()) {
            auto obj = q.front();
            q.pop();
            
            for(auto c : obj->children()) {
                if(c->type() == Type::SINGLETON)
                    c = ((SingletonObject*)c)->ptr();
                if(c->type() == Type::SECTION)
                    q.push((SectionInterface*)c);
                else if(c->type() == Type::ENTANGLED)
                    q.push((SectionInterface*)c);
                else {
                    Print("Clear cache on ", *c);
                    c->clear_cache();
                }
            }
            
            obj->clear_cache();
        }*/
        
        _graph = nullptr;
        TextureCache::remove_base(this);
        
        while(!_exec_main_queue.empty()) {
            (_exec_main_queue.front())();
            _exec_main_queue.pop();
        }
        
        base_pointers.erase(_platform->window_handle());
    }

    void IMGUIBase::set_background_color(const Color& color) {
        if(_platform)
            _platform->set_clear_color(color);
    }

    void IMGUIBase::event(const gui::Event &e) {
        if(_graph->event(e) && e.type != EventType::MMOVE)
            return;
        
        try {
            _event_fn(*_graph, e);
        } catch(const std::invalid_argument& e) { }
    }

    void IMGUIBase::loop() {
        LoopStatus status = LoopStatus::IDLE;
#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg([](void *data) {
            //static Timer timer;
            //Print("Frame timing ", timer.elapsed());
            //timer.reset();

            static std::once_flag flag;
            std::call_once(flag, []() {
                emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
            });

            auto self = ((IMGUIBase*)data);
            auto width = canvas_get_width();
            auto height = canvas_get_height();
            if (width != self->_graph->width() || height != self->_graph->height()) {
                Event event(WINDOW_RESIZED);

                ::cmn::gui::set_window_size(self->_platform->window_handle(), Vec2( width, height ));
                //glfwSetWindowSize(self->_platform->window_handle(), width, height );

                event.size.width = width;
                event.size.height = height;

                self->_graph->event(event);
            }

            auto status = ((IMGUIBase*)data)->update_loop();
            if (status == LoopStatus::END)
                emscripten_cancel_main_loop();
            //emscripten_sleep(16);
        }, (void*)this, 0 /* fps */, 1 /* simulate_infinite_loop */);

        //while(status != LoopStatus::END)
        //    emscripten_sleep(16);
        
#else
        // Main loop
        while (status != LoopStatus::END)
        {
            status = update_loop();
            if(status != gui::LoopStatus::UPDATED)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
#endif
    }

    LoopStatus IMGUIBase::update_loop() {
        {
            std::unique_lock<std::mutex> guard(_mutex);
            while (!_exec_main_queue.empty()) {
                auto fn = std::move(_exec_main_queue.front());
                
                guard.unlock();
                (fn)();
                guard.lock();
                _exec_main_queue.pop();
            }
        }
        
        return _platform->update_loop(_custom_loop);
    }

    void IMGUIBase::set_frame_recording(bool v) {
        _frame_recording = v;
        if(v) {
            _platform->set_frame_capture_enabled(true);
        } else if(_platform->frame_capture_enabled())
            _platform->set_frame_capture_enabled(false);
    }

    void IMGUIBase::set_last_framebuffer(Size2 size) {
        if (size.width > 0 && size.height > 0 
            && (size.width * _dpi_scale != _last_framebuffer_size.width 
                || size.height * _dpi_scale != _last_framebuffer_size.height))
        {
#ifndef NDEBUG
            //Print("Changed framebuffer size to ", fw,"x",fh);
#endif
            _last_framebuffer_size = size.mul(_dpi_scale);
            _graph->set_dialog_window_size(window_dimensions().div(_graph->scale()) * gui::interface_scale());
        }
    }

    void IMGUIBase::paint(DrawStructure& s) {
        int fw, fh;
        auto window = _platform->window_handle();
        
        std::call_once(_resize_init_flag, [this](){
            auto size = get_window_bounds().size();
            set_window_size(size + Size2(1,1));
            set_window_size(size - Size2(1,1));
        });
        
        //glfwGetFramebufferSize(window, &fw, &fh);
        auto fb = get_frame_buffer_size(window, 1);
        fw = fb.width;
        fh = fb.height;

        //fw *= _dpi_scale;
        //fh *= _dpi_scale;
        
        set_last_framebuffer(Size2(fw, fh));
        
        //auto lock = GUI_LOCK(s.lock());
        auto objects = s.collect();
        _objects_drawn = 0;
        _skipped = 0;
#ifndef NDEBUG
        _type_counts.clear();
#endif
        _draw_order.clear();
        _above_z.clear();
        _rotation_starts.clear();
        
        for(auto &o : objects)
            redraw(o, _draw_order, _above_z);
        
        std::sort(_above_z.begin(), _above_z.end(), [](const auto& A, const auto& B) {
            return A.ptr->z_index() < B.ptr->z_index() || (A.ptr->z_index() == B.ptr->z_index() && A.index < B.index);
        });
        
        for(auto & order : _draw_order) {
            draw_element(order);
        }

        for (auto& order : _above_z) {
            draw_element(order);
        }
        
#ifndef NDEBUG
        if(_last_debug_print.elapsed() > 60) {
            auto str = Meta::toStr(_type_counts);
            Print(_objects_drawn," drawn, ",_skipped,"skipped, types: ",str);
            _last_debug_print.reset();
        }
#endif
    }

void CalculateBoundingBox(const std::vector<ImVec2>& poly, ImVec2& min, ImVec2& max) {
    min.x = min.y = FLT_MAX;
    max.x = max.y = -FLT_MAX;
    for (const auto& p : poly) {
        if (p.x < min.x) min.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.x > max.x) max.x = p.x;
        if (p.y > max.y) max.y = p.y;
    }
}

void InsertIntersection(std::vector<ImVec2>& scanHits, const ImVec2& intersect) {
    scanHits.insert(std::lower_bound(scanHits.begin(), scanHits.end(), intersect, [](const ImVec2& a, const ImVec2& b) {
        return a.x < b.x;
    }), intersect);
}

void PolyFillScanFlood(ImDrawList *draw, const std::vector<ImVec2>& poly, std::vector<ImVec2>& output, ImU32 color, const int gap = 1, const int strokeWidth = 1) {
    using namespace std;
    
    ImVec2 min, max;
    CalculateBoundingBox(poly, min, max);
    
    const auto &io = ImGui::GetIO();
    
    // Vertically clip
    if (min.y < 0) min.y                = 0;
    if (max.y > io.DisplaySize.y) max.y = io.DisplaySize.y;
    
    // Bounds check
    if ((max.x < 0) || (min.x > io.DisplaySize.x) || (max.y < 0) || (min.y > io.DisplaySize.y)) return;
    
    // so we know we start on the outside of the object we step out by 1.
    min.x -= 1;
    max.x += 1;
    
    // Initialise our starting conditions
    int y = int(min.y);
    const auto polysize = poly.size();
    vector<ImVec2> scanHits;
    scanHits.reserve(polysize / 2);
    output.reserve(polysize * gap);
    
    // Go through each scan line iteratively, jumping by 'gap' pixels each time
    while (y < max.y) {
        scanHits.clear();
        
        for (size_t i = 0; i < polysize - 1; i++) {
            const ImVec2 pa = poly[i];
            const ImVec2 pb = poly[i + 1];
            
            // Skip double/dud points
            if (pa.x == pb.x && pa.y == pb.y) continue;
            
            // Test to see if this segment makes the scan-cut
            if ((pa.y > pb.y && y < pa.y && y > pb.y) || (pa.y < pb.y && y > pa.y && y < pb.y)) {
                ImVec2 intersect;
                intersect.y = y;
                if (pa.x == pb.x) {
                    intersect.x = pa.x;
                } else {
                    intersect.x = (pb.x - pa.x) / (pb.y - pa.y) * (y - pa.y) + pa.x;
                }
                
                // Add intersection while keeping scanHits sorted
                InsertIntersection(scanHits, intersect);
            }
        }
        
        // Generate the line segments
        {
            auto l = scanHits.size(); // We need pairs of points; this prevents segfault
            for (size_t i = 0; i + 1 < l; i += 2) {
                output.push_back(scanHits[i]);
                output.push_back(scanHits[i + 1]);
                draw->AddLine(scanHits[i], scanHits[i + 1], color, strokeWidth);
            }
        }
        
        y += gap;
    } // For each scan line
}

void ImRotateStart(int& rotation_start_index, ImDrawList* list)
{
    rotation_start_index = list->VtxBuffer.Size;
}

ImVec2 ImRotationCenter(int rotation_start_index, ImDrawList* list)
{
    ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX); // bounds

    const auto& buf = list->VtxBuffer;
    for (int i = rotation_start_index; i < buf.Size; i++)
        l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);

    return ImVec2((l.x+u.x)/2, (l.y+u.y)/2); // or use _ClipRectStack?
}

ImVec2 operator-(const ImVec2& l, const ImVec2& r) { return{ l.x - r.x, l.y - r.y }; }

void ImRotateEnd(int rotation_start_index, ImDrawList* list, Float2_t rad, ImVec2 center)
{
    //auto center = ImRotationCenter(list);
    Float2_t s=cos(rad), c=sin(rad);
    center = ImRotate(center, s, c) - center;

    auto& buf = list->VtxBuffer;
    for (int i = rotation_start_index; i < buf.Size; i++)
        buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
}

bool operator!=(const ImVec4& A, const ImVec4& B) {
    return A.w != B.w || A.x != B.x || A.y != B.y || A.z != B.z;
}

std::string u8string_to_string(const std::u8string& u8str)
{
    std::string str(u8str.begin(), u8str.end());
    return str;
}

uint32_t blendColors(uint32_t color1, uint32_t color2) {
    if (color1 == color2)
        return color1;

    // Fast exit for fully transparent colors
    if ((color1 >> 24) == 0) return color2;
    if ((color2 >> 24) == 0) return color1;

    // Extract and pre-multiply alpha
    uint32_t alpha1 = (color1 >> 24) & 0xFF;
    uint32_t alpha2 = (color2 >> 24) & 0xFF;
    uint32_t premulAlpha1 = alpha1 + 1;
    uint32_t premulAlpha2 = alpha2 + 1;

    // Extract colors and pre-multiply by alpha
    uint32_t rb1 = (color1 & 0xFF00FF) * premulAlpha1;
    uint32_t rb2 = (color2 & 0xFF00FF) * premulAlpha2;
    uint32_t g1 = (color1 & 0x00FF00) * premulAlpha1;
    uint32_t g2 = (color2 & 0x00FF00) * premulAlpha2;

    // Blend colors and alpha
    uint32_t rb = ((rb1 + rb2) >> 8) & 0xFF00FF;
    uint32_t g = ((g1 + g2) >> 8) & 0x00FF00;
    uint32_t blendedAlpha = std::min(255u, alpha1 + alpha2);

    // Combine blended components into a single color
    return (blendedAlpha << 24) | rb | g;
}


void RenderNonAntialiasedStroke(const std::vector<Vertex>& points, const IMGUIBase::DrawOrder& order, ImDrawList* list, Float2_t thickness) {
    const auto points_count = points.size();

    if (points_count <= 1) {
        return;
    }
    //thickness = 5;
    /*std::vector<Vertex> converted(points.size());
    for(size_t i=0; i<points.size(); ++i) {
        converted[i] = Vertex(order.transform.transformPoint(points[i].position()), points[i].color());
    }*/
    //AddPolyline(list, converted.data(), converted.size(), false, thickness);
    //return;
    
    const auto count = points_count - 1;
    const ImVec2 uv = list->_Data->TexUvWhitePixel;

    const auto idx_count = count * 6;
    const auto vtx_count = count * 4; //! TODO: [OPT] Not sharing edges
    list->PrimReserve(static_cast<int>(idx_count), static_cast<int>(vtx_count));
    assert(idx_count > 0 and vtx_count > 0);
    auto p1 = order.transform.transformPoint(points.front().position());

    for (size_t i1 = 0; i1 < count; i1++) {
        const size_t i2 = (i1 + 1) == points_count ? 0 : i1 + 1;
        const auto p2 = order.transform.transformPoint(points[i2].position());
        const auto col1 = points[i1].color();
        const auto col2 = points[i2].color();
        //const auto col = blendColors(col1, col2);

        Float2_t dx = p2.x - p1.x;
        Float2_t dy = p2.y - p1.y;
        IM_NORMALIZE2F_OVER_ZERO(dx, dy);
        dx *= (thickness * 0.5_F);
        dy *= (thickness * 0.5_F);

        // Vertex Writing
        list->_VtxWritePtr[0].pos.x = p1.x + dy; list->_VtxWritePtr[0].pos.y = p1.y - dx; list->_VtxWritePtr[0].uv = uv; list->_VtxWritePtr[0].col = col1;
        list->_VtxWritePtr[1].pos.x = p2.x + dy; list->_VtxWritePtr[1].pos.y = p2.y - dx; list->_VtxWritePtr[1].uv = uv; list->_VtxWritePtr[1].col = col2;
        list->_VtxWritePtr[2].pos.x = p2.x - dy; list->_VtxWritePtr[2].pos.y = p2.y + dx; list->_VtxWritePtr[2].uv = uv; list->_VtxWritePtr[2].col = col2;
        list->_VtxWritePtr[3].pos.x = p1.x - dy; list->_VtxWritePtr[3].pos.y = p1.y + dx; list->_VtxWritePtr[3].uv = uv; list->_VtxWritePtr[3].col = col1;
        list->_VtxWritePtr += 4;

        // Index Writing
        list->_IdxWritePtr[0] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx);
        list->_IdxWritePtr[1] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx + 1);
        list->_IdxWritePtr[2] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx + 2);
        list->_IdxWritePtr[3] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx);
        list->_IdxWritePtr[4] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx + 2);
        list->_IdxWritePtr[5] = static_cast<ImDrawIdx>(list->_VtxCurrentIdx + 3);
        list->_IdxWritePtr += 6;
        list->_VtxCurrentIdx += 4;
        
        p1 = p2;
    }

    list->_Path.Size = 0;
}



void IMGUIBase::draw_element(const DrawOrder& order) {
    auto list = ImGui::GetForegroundDrawList();
    /*if(order.type == DrawOrder::POP) {
        if(list->_ClipRectStack.size() > 1) {
            //list->PopClipRect();
        } else
            FormatWarning("Cannot pop too many rects.");
        return;
    }*/
    
    if(order.type == DrawOrder::END_ROTATION) {
        auto o = order.ptr;
        assert(o->has_global_rotation());
        if(!_rotation_starts.count(o)) {
            FormatWarning("Cannot find object.");
            return;
        }
        auto && [rotation_start, center] = _rotation_starts.at(o);
        ImRotateEnd(rotation_start, list, o->rotation(), center);
        return;
    }
    
    //auto i_ = list->VtxBuffer.Size;
    auto o = order.ptr;
    Vec2 center;
    auto bds = order.bounds;
    auto &io = ImGui::GetIO();
    Transform transform(order.transform);
    
    int rotation_start;
    
    if(order.type == DrawOrder::START_ROTATION) {
        ImRotateStart(rotation_start, list);
        
        // generate position without rotation
        Vec2 scale = (_graph->scale() / gui::interface_scale() / _dpi_scale) .div(Vec2(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y));
        
        transform = Transform();
        transform.scale(scale);
        transform.combine(o->parent()->global_transform());
        transform.translate(o->pos());
        transform.scale(o->scale());
        transform.translate(-o->size().mul(o->origin()));
        
        bds = transform.transformRect(Bounds(Vec2(), o->size()));
        center = bds.pos() + bds.size().mul(o->origin());
        
        if(o->type() == Type::ENTANGLED) {
            _rotation_starts[o] = { rotation_start, center };
        }
        
        return;
    }
    
    if(not o->was_visible())
        return;
    
    ++_objects_drawn;
    auto cache = o->cached(this);
    
    if(o->rotation() && o->type() != Type::ENTANGLED) {
        ImRotateStart(rotation_start, list);
        
        // generate position without rotation
        Vec2 scale = (_graph->scale() / gui::interface_scale() / _dpi_scale) .div(Vec2(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y));
        
        transform = Transform();
        transform.scale(scale);
        transform.combine(o->parent()->global_transform());
        transform.translate(o->pos());
        transform.scale(o->scale());
        transform.translate(-o->size().mul(o->origin()));
        
        bds = transform.transformRect(Bounds(Vec2(), o->size()));
        center = bds.pos() + bds.size().mul(o->origin());
    }
    
    bool pushed_rect = false;
    //if(order._clip_rect.w > 0 && order._clip_rect.z > 0 && (list->_ClipRectStack.empty() || list->_ClipRectStack.back() != order._clip_rect))
    if(order._clip_rect.w > 0 && order._clip_rect.z > 0) {
        //list->AddRect(ImVec2(order._clip_rect.x, order._clip_rect.y),
        //              ImVec2(order._clip_rect.w, order._clip_rect.z), cvtClr(Red));
        list->PushClipRect(ImVec2(order._clip_rect.x, order._clip_rect.y),
                           ImVec2(order._clip_rect.w, order._clip_rect.z), false);
        pushed_rect = true;
    }
    
    Float2_t global_scale = (transform.transformPoint(Vec2(1)) - transform.transformPoint(Vec2(0))).x;
    
    switch (o->type()) {
        case Type::CIRCLE: {
            auto ptr = static_cast<Circle*>(o);
            
            // calculate a reasonable number of segments based on global bounds
            auto e = 0.25_F;
            auto r = max(1, order.bounds.width * 0.5_F);
            auto th = acos(2 * SQR(1 - e / r) - 1);
            int64_t num_segments = (int64_t)ceil(2*M_PI/th);
            
            if(num_segments <= 1)
                break;
            
            // generate circle path
            auto centre = ImVec2(ptr->radius(), ptr->radius());
            const Float2_t a_max = (Float2_t)M_PI*2.0_F * ((Float2_t)num_segments - 1.0_F) / (Float2_t)num_segments;
            
            list->PathArcTo(centre, ptr->radius(), 0.0_F, a_max, (int)num_segments - 1);
            
            // transform according to local transform etc.
            for (auto i=0; i<list->_Path.Size; ++i) {
                list->_Path.Data[i] = order.transform.transformPoint(list->_Path.Data[i]);
            }
            
            // generate vertices (1. filling + 2. outline)
            if(ptr->fill_clr() != Transparent)
                list->AddConvexPolyFilled(list->_Path.Data, list->_Path.Size, (ImColor)ptr->fill_clr());
            if(ptr->line_clr() != Transparent)
                list->AddPolyline(list->_Path.Data, list->_Path.Size, (ImColor)ptr->line_clr(), true, 1);
            
            // reset path
            list->_Path.Size = 0;
            
            break;
        }
            
        case Type::POLYGON: {
            auto ptr = static_cast<Polygon*>(o);
            static std::vector<ImVec2> points;
            if(ptr->relative()) {
                points.clear();
                points.reserve(ptr->relative()->size());
                
                ImVec2 prev{order.transform.transformPoint(ptr->relative()->back())};
                for(auto &pt : *ptr->relative()) {
                    auto cvt = order.transform.transformPoint(pt);
                    if(sqdistance(cvt, prev) > SQR(5)) {
                        points.emplace_back(std::move(cvt));
                        prev = cvt;
                    }
                }
                
                //Print("points size = ", points.size(), " vs. ", ptr->relative()->size());
                
                if(points.size() >= 3) {
                    points.push_back(points.front());
                    static std::vector<ImVec2> output;
                    output.clear();
                    
                    if(!cache) {
                        cache = o->insert_cache(this, std::make_unique<PolyCache>()).get();
                    }
                    
                    //const auto& original = ((PolyCache*)cache)->original();
                    if(cache->changed()
                       //&& (points.size() != original.size()
                       //    || not std::equal(points.begin(), points.end(), original.begin(), [](const ImVec2& A, const ImVec2& B) { return int(A.x) == int(B.x) && int(A.y) == int(B.y); }))
                       )
                    {
                        //((PolyCache*)cache)->original() = points;
                        PolyFillScanFlood(list, points, output, cvtClr(ptr->fill_clr()));
                        ((PolyCache*)cache)->points() = output;
                        cache->set_changed(false);
                    } else {
                        auto& output = ((PolyCache*)cache)->points();
                        auto fill = (uint32_t)((ImColor)ptr->fill_clr());
                        for(size_t i=0; i<output.size(); i+=2) {
                            list->AddLine(output[i], output[i+1], fill, 1);
                        }
                    }
                    
                    if(ptr->show_points()) {
                        auto clr = cvtClr(ptr->border_clr());
                        for(auto &pt : *ptr->relative()) {
                            auto cvt = order.transform.transformPoint(pt);
                            list->AddCircle(cvt, order.transform.transformPoint(1, 1).x - order.transform.transformPoint(0, 0).x, clr);
                        }
                    }
                    
                } else if(cache) {
                    o->remove_cache(this);
                }
                
                //list->AddConvexPolyFilled(points.data(), points.size(), (ImColor)ptr->fill_clr());
                if(ptr->border_clr() != Transparent)
                    list->AddPolyline(points.data(), (int)points.size(), (ImColor)ptr->border_clr(), true, 1);
            }
            
            break;
        }
            
        case Type::TEXT: {
            auto ptr = static_cast<Text*>(o);
            
            if(ptr->txt().empty())
                break;
            
            auto font = _fonts.at(ptr->font().style);
            auto font_scale = ptr->global_text_scale().x * font->FontSize * (ptr->font().size / im_font_scale / _dpi_scale / io.DisplayFramebufferScale.x);
            
            Vec2 rounded(round(bds.x), round(bds.y));
            //Print(ptr->txt(), "scale = ", font_scale, " rounded = ", Vec2(rounded), " (", bds.pos(),")");
            
            if(ptr->shadow() > 0) {
                Vec2 offset = Vec2(1.5_F * global_scale + font->ConfigData->GlyphOffset.y / font_scale);//.map([](auto x){return round(x);});
                
                list->AddText(font,
                              font_scale,
                              bds.pos() + offset,
                              (ImColor)Color(20, 20, 20, 255 * saturate(ptr->shadow(), 0_F, 1_F)),
                              ptr->txt().c_str());
            }
            list->AddText(font,
                          font_scale,
                          bds.pos(),
                          (ImColor)ptr->color(),
                          ptr->txt().c_str());
            
            break;
        }
            
        case Type::ENTANGLED: {
            //list->AddRect(ImVec2(bds.x, bds.y), ImVec2(bds.x + bds.width, bds.y + bds.height), cvtClr(Red));
            //list->PushClipRect(ImVec2(bds.x, bds.y), ImVec2(bds.x + bds.width, bds.y + bds.height), false);
            
            break;
        }
            
        case Type::IMAGE: {
            if(!cache) {
                auto tex_cache = TextureCache::get(this);
                cache = tex_cache.get();
                tex_cache->set_object(o);
                o->insert_cache(this, std::move(tex_cache));
                
                //tex_cache->platform() = _platform.get();
                //tex_cache->set_base(this);
                
                
                //_all_textures[this].insert(tex_cache);
            }
            
            if(!static_cast<ExternalImage*>(o)->source())
                break;
            
            auto image_size = static_cast<ExternalImage*>(o)->source()->bounds().size();
            if(image_size.empty() || image_size.width == 0 || image_size.height == 0)
                break;
            
            auto tex_cache = (TextureCache*)cache;
            tex_cache->update_with(static_cast<ExternalImage*>(o));
            tex_cache->set_changed(false);
            
            ImU32 col = IM_COL32_WHITE;
            uchar a = static_cast<ExternalImage*>(o)->color().a;
            if(a > 0 && static_cast<ExternalImage*>(o)->color() != White)
                col = (ImColor)static_cast<ExternalImage*>(o)->color();
            
            auto I = list->VtxBuffer.size();
            /*if(static_cast<ExternalImage*>(o)->cut_border()) {
                list->AddImage(tex_cache->texture()->ptr,
                               ImVec2(0, 0),
                               ImVec2(o->width()-1, o->height()-1),
                               ImVec2(0, 0),
                               ImVec2((tex_cache->texture()->image_width-1) / float(tex_cache->texture()->width),
                                      (tex_cache->texture()->image_height-1) / float(tex_cache->texture()->height)),
                               col);
            } else {*/
                list->AddImage((ImTextureID)tex_cache->texture()->ptr,
                               ImVec2(0, 0),
                               ImVec2(o->width() - 0.5, o->height() - 0.5),
                               ImVec2(0, 0),
                               ImVec2((o->width() - 0.5) / Float2_t(tex_cache->texture()->width),
                                      (o->height() - 0.5) / Float2_t(tex_cache->texture()->height)),
                               col);
            //}
            for (auto i = I; i<list->VtxBuffer.size(); ++i) {
                list->VtxBuffer[i].pos = transform.transformPoint(list->VtxBuffer[i].pos);
            }
            break;
        }
            
        case Type::RECT: {
            auto ptr = static_cast<Rect*>(o);
            auto &rect = order.bounds;
            
            if(rect.size().empty())
                break;
            
            if(ptr->fillclr().a > 0)
                list->AddRectFilled((ImVec2)transform.transformPoint(Vec2()),
                                    (ImVec2)transform.transformPoint(o->size()),
                                    cvtClr(ptr->fillclr()));
            
            if(ptr->lineclr().a > 0)
                list->AddRect((ImVec2)transform.transformPoint(Vec2()),
                              (ImVec2)transform.transformPoint(o->size()),
                              cvtClr(ptr->lineclr()));
            
            break;
        }
            
        case Type::LINE:
        case Type::VERTICES: {
            auto ptr = static_cast<VertexArray*>(o);
            
            // Non Anti-aliased Stroke
            auto &points = ptr->VertexArray::points();
            /*    list->AddRect((ImVec2)transform.transformPoint(Vec2()),
                              (ImVec2)transform.transformPoint(o->size()),
                              cvtClr(o->was_visible() ? Purple : Green));*/
            RenderNonAntialiasedStroke(points, order, list, ptr->thickness());
            break;
        }
            
        default:
            break;
    }
    
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
    if constexpr(std::same_as<default_impl_t, MetalImpl>) {
        if(static_cast<const MetalImpl*>(_platform.get())->gui_macos_blur()) {
            bool blur = false;
            auto p = o;
            while(p) {
                if(p->tagged(Effects::blur)) {
                    blur = true;
                    break;
                }
                
                p = p->parent();
            }
            
            auto e = list->VtxBuffer.Size;
            for(auto i=i_; i<e; ++i) {
                list->VtxBuffer[i].mask = blur;
            }
        }
    }
#endif
    
    if(!list->CmdBuffer.empty()) {
        if(list->CmdBuffer.back().ElemCount == 0) {
            (void)list->CmdBuffer;
        }
    }
    
#define DEBUG_BOUNDARY
#ifdef DEBUG_BOUNDARY
    if(graph()->is_key_pressed(Codes::RShift)) {
        list->AddRect(bds.pos(), bds.pos() + bds.size(), (ImColor)Red.alpha(125));
        std::string text;
        if(o->parent() && o->parent()->background() == o) {
            if(dynamic_cast<Entangled*>(o->parent()))
                text = dynamic_cast<Entangled*>(o->parent())->name() + " " + Meta::toStr(o->parent()->bounds());
            else
                text = Meta::toStr(*(Drawable*)o->parent());
        } else
            text = Meta::toStr(*o) + " "+ Meta::toStr(o->bounds());
        auto font = _fonts.at(Style::Regular);
        auto _font = Font(0.3, Style::Regular);
        
        list->AddText(font, font->FontSize * (_font.size / im_font_scale / _dpi_scale / io.DisplayFramebufferScale.x), bds.pos(), (ImColor)White.alpha(200), text.c_str());
    }
#endif
    
    if(o->type() != Type::ENTANGLED && o->has_global_rotation()) {
        ImRotateEnd(rotation_start, list, o->rotation(), center);
    }
    
    if(pushed_rect) {
        assert(!list->_ClipRectStack.empty());
        list->PopClipRect();
    }
    
#if false
    list->AddRectFilled((ImVec2)transform.transformPoint(Vec2()),
                  (ImVec2)transform.transformPoint(o->size()),
                  cvtClr(Black.alpha(100)));
    list->AddRect((ImVec2)transform.transformPoint(Vec2()),
                  (ImVec2)transform.transformPoint(o->size()),
                  cvtClr(Red));
    
    auto font = _fonts.at(Style::Regular);
    auto str = Meta::toStr(o->bounds().pos().map(std::roundf))+ " p:" + Meta::toStr(o->global_bounds().pos().map(std::roundf))+" scale:"+Meta::toStr(o->scale())+" "+Meta::toStr((int*)o->parent());
    list->AddText(font, 0.5 * font->FontSize * (1.f / im_font_scale / _dpi_scale / io.DisplayFramebufferScale.x), bds.pos(), (ImColor)Green, str.c_str());
#endif
    
}

    void IMGUIBase::redraw(Drawable *o, std::vector<DrawOrder>& draw_order, std::vector<DrawOrder>& above_z, bool is_background, ImVec4 clip_rect) {
        if (!o)
            return;

        static auto entangled_will_texture = [](Entangled* e) {
            assert(e);
            if(e->scroll_enabled() && e->size().max() > 0) {
                return true;
            }
            
            return false;
        };
        
        if(o->type() == Type::SINGLETON)
            o = static_cast<SingletonObject*>(o)->ptr();
        o->set_was_visible(false);
        
        auto &io = ImGui::GetIO();
        Vec2 scale = (_graph->scale() / gui::interface_scale() / _dpi_scale) .div(Vec2(io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y));
        Transform transform;
        transform.scale(scale);
        
        if(is_background && o->parent() && o->parent()->type() == Type::ENTANGLED)
        {
            auto p = o->parent();
            if(p)
                transform.combine(p->global_transform());
            
        } else {
            transform.combine(o->global_transform());
        }
        
        auto bounds = transform.transformRect(Bounds(0, 0, o->width(), o->height()));
        
        //bool skip = false;
        auto dim = window_dimensions() / _dpi_scale;
        
        if(!Bounds(0, 0, dim.width-0, dim.height-0).overlaps(bounds)
           && (o->type() != Type::ENTANGLED /*|| dynamic_cast<PlaceinLayout*>(o) == nullptr*/) && (o->type() != Type::SECTION)) {
            ++_skipped;
            return;
        } else
        
        o->set_was_visible(true);
#ifndef NDEBUG
        ++_type_counts[o->type()];
#endif
        
        switch (o->type()) {
            case Type::ENTANGLED: {
                auto ptr = static_cast<Entangled*>(o);
                if(ptr->rotation() != 0)
                    draw_order.emplace_back(DrawOrder::START_ROTATION, draw_order.size() + above_z.size(), o, transform, bounds, clip_rect);
                
                auto bg = static_cast<Entangled*>(o)->background();
                if(bg) {
                    redraw(bg, draw_order, above_z, true, clip_rect);
                }
                
                assert(!ptr->begun());
                if(entangled_will_texture(ptr)) {
                    clip_rect = bounds;
                    
                    //draw_order.emplace_back(DrawOrder::DEFAULT, draw_order.size(), ptr, transform, bounds, clip_rect);
                    
                    for(auto c : ptr->children()) {
                        if (!c)
                            continue;

                        if(ptr->scroll_enabled()) {
                            auto b = c->local_bounds();
                            
                            //! Skip drawables that are outside the view
                            //  TODO: What happens to Drawables that dont have width/height?
                            Float2_t x = b.x;
                            Float2_t y = b.y;
                            
                            if(y < -b.height || y > ptr->height()
                               || x < -b.width || x > ptr->width())
                            {
                                continue;
                            }
                        }
                        
                        redraw(c, draw_order, above_z, false, clip_rect);
                    }
                    
                    //draw_order.emplace_back(DrawOrder::POP, draw_order.size(), ptr, transform, bounds, clip_rect);
                    
                } else {
                    for(auto c : ptr->children())
                        redraw(c, draw_order, above_z, false, clip_rect);
                }
                
                if(ptr->rotation() != 0) {
                    draw_order.emplace_back(DrawOrder::END_ROTATION, draw_order.size() + above_z.size(), ptr, transform, bounds, clip_rect);
                }
                
                break;
            }
                
            default:
                if(o->z_index() != 0)
                    above_z.emplace_back(DrawOrder::DEFAULT, draw_order.size() + above_z.size(), o, transform, bounds, clip_rect);
                else
                    draw_order.emplace_back(DrawOrder::DEFAULT, draw_order.size() + above_z.size(), o, transform, bounds, clip_rect);
                break;
        }
    }

    Bounds IMGUIBase::text_bounds(const std::string& text, Drawable* obj, const Font& font) {
        if(_fonts.empty()) {
            FormatWarning("Trying to retrieve text_bounds before fonts are available.");
            return gui::Base::text_bounds(text, obj, font);
        }
        
        auto imfont = _fonts.at(font.style);
        Vec2 size = imfont->CalcTextSizeA(imfont->FontSize * font.size / im_font_scale, FLT_MAX, -1_F, text.c_str(), text.c_str() + text.length(), NULL);
        // Round
        //size.x = max(0, (float)(int)(size.x - 0.95_F));
        //size.y = line_spacing(font);
        //size.y = max(0, (float)(int)(size.y - 0.95_F));
        //return text_size;
        //auto size = ImGui::CalcTextSize(text.c_str());
        return Bounds(Vec2(), size);
    }

    Float2_t IMGUIBase::line_spacing(const Font& font) {
        if(_fonts.empty()) {
            FormatWarning("Trying to get line_spacing without a font loaded.");
            return Base::line_spacing(font);
        }
        return font.size * _fonts.at(font.style)->FontSize / im_font_scale;
    }

    void IMGUIBase::set_title(std::string title) {
        _title = title;
        
        exec_main_queue([this, title](){
            if(_platform)
                _platform->set_title(title);
        });
    }

    Size2 IMGUIBase::window_dimensions() const {
        //auto window = _platform->window_handle();
        //int width, height;
        //glfwGetWindowSize(window, &width, &height);
        
        
        //glfwGetFramebufferSize(window, &width, &height);
        //Size2 size(width, height);
        
        return _last_framebuffer_size;
    }

Size2 IMGUIBase::real_dimensions() {
    if(_dpi_scale > 0)
        return _last_framebuffer_size.div(_dpi_scale);
    return _last_framebuffer_size;
}

    Event IMGUIBase::toggle_fullscreen(DrawStructure &graph) {
        static int _wndPos[2];
        
        Print("Enabling full-screen.");
        _platform->toggle_full_screen();
        
        Event event(WINDOW_RESIZED);
        
        // backup window position and window size
        glfwGetWindowPos( _platform->window_handle(), &_wndPos[0], &_wndPos[1] );
        auto fb = get_frame_buffer_size(_platform->window_handle(), get_scale_multiplier());

        event.size.width = fb.width;
        event.size.height = fb.height;
        graph.event(event);
        
        return event;
    }

void IMGUIBase::draw_loop() {
    //! draw loop
    if(_graph == NULL)
        return;
    
    //auto lock = GUI_LOCK(_graph->lock());
    this->paint(*_graph);
    
    auto cache = _graph->root().cached(this);
    if(cache)
        cache->set_changed(false);
}

bool IMGUIBase::new_frame_function() {
    //! new frame function, tells the drawing system whether an update is required
    //auto lock = GUI_LOCK(_graph->lock());
    _graph->before_paint(this);
    
    auto cache = _graph->root().cached(this);
    if(!cache) {
        cache = _graph->root().insert_cache(this, std::make_unique<CacheObject>()).get();
    }
    
    return cache->changed() || _graph->root().is_animating();
}

}
