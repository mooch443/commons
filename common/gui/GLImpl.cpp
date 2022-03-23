#if COMMONS_HAS_OPENGL || defined(__EMSCRIPTEN__)
#include <types.h>
#include <cstdio>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#if !defined(__EMSCRIPTEN__)
#include <imgui/backends/imgui_impl_opengl2.h>
using ImTextureID_t = ImGui_OpenGL2_TextureID;
#endif

#include <imgui/backends/imgui_impl_opengl3.h>
#if defined(__EMSCRIPTEN__)
using ImTextureID_t = ImGui_OpenGL3_TextureID;
#endif

#if defined(__EMSCRIPTEN__)
#define IMGUI_IMPL_OPENGL_ES3
#define GLFW_INCLUDE_ES3
#define GL_VERSION_3_2
#include <GLES3/gl3.h>
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>    // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#if !defined(GL_VERSION_3_2) && !defined(__EMSCRIPTEN__)
#define OPENGL3_CONDITION (false)
#elif defined(__EMSCRIPTEN__)
#define OPENGL3_CONDITION (true)
#else
#define OPENGL3_CONDITION (!CMN_USE_OPENGL2 && ((GLVersion.major == 3 && GLVersion.minor >= 2) || (GLVersion.major > 3)))
#endif

#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0
#endif
#ifndef glMapBuffer
#define glMapBuffer
#endif
#ifndef GL_BGRA
#define GL_BGRA GL_RGBA
#endif
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0
#endif
#ifndef GL_RG
#define GL_RG 0
#endif
#ifndef GL_RG8
#define GL_RG8 0
#endif
#ifndef GL_TEXTURE_SWIZZLE_RGBA
#define GL_TEXTURE_SWIZZLE_RGBA 0
#endif

//#define GLFW_INCLUDE_GL3  /* don't drag in legacy GL headers. */
#define GLFW_NO_GLU       /* don't drag in the old GLU lib - unless you must. */

#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#include "GLImpl.h"
#include <misc/Timer.h>
#include <misc/checked_casts.h>

#ifdef WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3native.h>
#undef GLFW_EXPOSE_NATIVE_WIN32

#include <gui/darkmode.h>
#endif

#define GLIMPL_CHECK_THREAD_ID() check_thread_id( __LINE__ , __FILE__ )

//#include "misc/freetype/imgui_freetype.h"
//#include "misc/freetype/imgui_freetype.cpp"

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

namespace gui {

#if !defined(NDEBUG) || true
void checkGLError(cmn::source_location loc = cmn::source_location::current())
{
    GLenum err;
    const char* buffer = "";
#if !defined(__EMSCRIPTEN__)
    while ((err = glfwGetError(&buffer)) != GL_NO_ERROR) {
#else
    while ((err = glGetError()) != GL_NO_ERROR) {


#endif
        print(loc.file_name(), ":", loc.line(), " [GL]: ", err, ": '", (const char*)buffer, "'.");
    }
}
#else
#define checkGLError()
#endif

GLImpl::GLImpl(std::function<void()> draw, std::function<bool()> new_frame_fn) 
    : texture_mutex(std::make_shared<std::mutex>()), _texture_updates(std::make_shared<decltype(_texture_updates)::element_type>()), draw_function(draw), new_frame_fn(new_frame_fn)
{
}

void GLImpl::init() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw U_EXCEPTION("[GL] Cannot initialize GLFW.");
    
    draw_calls = 0;
    _update_thread = std::this_thread::get_id();
}

void GLImpl::post_init() {
    if OPENGL3_CONDITION {
        ImGui_ImplOpenGL3_NewFrame();
    } 
#if !defined(__EMSCRIPTEN__)
    else
        ImGui_ImplOpenGL2_NewFrame();
#endif
}

void GLImpl::set_icons(const std::vector<file::Path>& icons) {
    std::vector<GLFWimage> images;
    std::vector<Image::UPtr> data;

    for (auto& path : icons) {
        if (!path.exists()) {
            FormatExcept("Cant find application icon ",path.str(),".");
            continue;
        }

        cv::Mat image = cv::imread(path.str(), cv::IMREAD_UNCHANGED);
        if (image.empty())
            continue;

        assert(image.channels() <= 4 && image.channels() != 2);

        cv::cvtColor(image, image, image.channels() == 3 ? cv::COLOR_BGR2RGBA : (image.channels() == 4 ? cv::COLOR_BGRA2RGBA : cv::COLOR_GRAY2RGBA));

        auto ptr = Image::Make(image);
        data.emplace_back(std::move(ptr));
        images.push_back(GLFWimage());
        images.back().pixels = data.back()->data();
        images.back().width = sign_cast<int>(data.back()->cols);
        images.back().height = sign_cast<int>(data.back()->rows);
    }

    glfwSetWindowIcon(window, images.size(), images.data());
}

void GLImpl::create_window(const char* title, int width, int height) {
    glfwSetErrorCallback([](int code, const char* str) {
        FormatExcept("[GLFW] Error ", code,": '", str,"'");
    });

#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    
    #if !CMN_USE_OPENGL2
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #endif
    
#else
    #if defined(__EMSCRIPTEN__)
        width = 800;
        height = 600;
        glfwWindowHint(GLFW_MAXIMIZED, 1);
        glfwWindowHint(GLFW_RESIZABLE, 1);
        const char* glsl_version = "#version 300 es";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 0);

    #elif !CMN_USE_OPENGL2
        // GL 3.0 + GLSL 130
        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    #else
        // GL 2.1 + GLSL 120
        const char* glsl_version = "#version 110";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #endif
#endif
    
    // Create window with graphics context
    print("Creating window with dimensions ", width, "x", height, " and title ", std::string(title));
    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL)
        throw U_EXCEPTION("[GL] Cannot create GLFW window.");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize OpenGL loader
#if !defined(__EMSCRIPTEN__)
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        throw U_EXCEPTION("Failed to initialize OpenGL loader!");
    }
#endif
    
    if OPENGL3_CONDITION
        print("Using OpenGL3.2 (seems supported, ", (const char*)glGetString(GL_VERSION),").");
    else
        print("Using OpenGL2.1 (", (const char*)glGetString(GL_VERSION),")");
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    if OPENGL3_CONDITION
        ImGui_ImplOpenGL3_Init(glsl_version);
#if !defined(__EMSCRIPTEN__)
    else
        ImGui_ImplOpenGL2_Init();
#endif

#ifdef WIN32
    auto native = glfwGetWin32Window(window);
    std::once_flag once;
    std::call_once(once, []() {
        InitDarkMode();
        g_darkModeEnabled = true;
        AllowDarkModeForApp(true);
    });
    
    AllowDarkModeForWindow(native, true);
    RefreshTitleBarThemeColor(native);
#endif

    print("Init complete.");
}

GLFWwindow* GLImpl::window_handle() {
    return window;
}

LoopStatus GLImpl::update_loop(const CrossPlatform::custom_function_t& custom_loop) {
    LoopStatus status = LoopStatus::IDLE;
    glfwPollEvents();
    
    if(glfwWindowShouldClose(window))
        return LoopStatus::END;
    
    if(!custom_loop())
        return LoopStatus::END;
    
    if(new_frame_fn())
    {
        {
            std::lock_guard guard(*texture_mutex);
            for(auto & fn : *_texture_updates)
                fn();
            _texture_updates->clear();
        }

        if OPENGL3_CONDITION
            ImGui_ImplOpenGL3_NewFrame();
#if !defined(__EMSCRIPTEN__)
        else
            ImGui_ImplOpenGL2_NewFrame();
#endif
        ImGui_ImplGlfw_NewFrame();
        
        ImGui::NewFrame();
        draw_function();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        if(_frame_capture_enabled)
            init_pbo((uint)display_w, (uint)display_h);

        glClearColor(_clear_color.r / 255.f, _clear_color.g / 255.f, _clear_color.b / 255.f, _clear_color.a / 255.f);
        glClear(GL_COLOR_BUFFER_BIT);

        if OPENGL3_CONDITION {
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        } 
#if !defined(__EMSCRIPTEN__)
        else {
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        }
#endif

        if(_frame_capture_enabled)
            update_pbo();
        else if(pboImage) {
            pboImage = nullptr;
            pboOutput = nullptr;
        }

        if(!OPENGL3_CONDITION)
            glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);

        ++draw_calls;
        status = LoopStatus::UPDATED;
        
    }
    
    /*if(draw_timer.elapsed() >= 1) {
        print(draw_calls," draw_calls / s");
        draw_calls = 0;
        draw_timer.reset();
    }*/
    
    return status;
}

void GLImpl::init_pbo(uint width, uint height) {
    if(!pboImage || pboImage->cols != width || pboImage->rows != height) {
        if OPENGL3_CONDITION {
            if(pboImage) {
                glDeleteBuffers(2, pboIds);
            }
            
            pboImage = Image::Make(height, width, 4);
            pboOutput = Image::Make(height, width, 4);
            
            glGenBuffers(2, pboIds);
            auto nbytes = width * height * 4;
            for(int i=0; i<2; ++i) {
                glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[i]);
                glBufferData(GL_PIXEL_PACK_BUFFER, nbytes, NULL, GL_STREAM_READ);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }
}

void GLImpl::update_pbo() {
#if !COMMONS_NO_PYTHON
    if OPENGL3_CONDITION {
        // "index" is used to read pixels from framebuffer to a PBO
        // "nextIndex" is used to update pixels in the other PBO
        index = (index + 1) % 2;
        nextIndex = (index + 1) % 2;

        // set the target framebuffer to read
        glReadBuffer(GL_BACK);

        // read pixels from framebuffer to PBO
        // glReadPixels() should return immediately.
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[index]);
        glReadPixels(0, 0, (GLsizei)pboImage->cols, (GLsizei)pboImage->rows, GL_BGRA, GL_UNSIGNED_BYTE, 0);

        // map the PBO to process its data by CPU
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[nextIndex]);
        GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if(ptr)
        {
            memcpy(pboImage->data(), ptr, pboImage->size());
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            
            // flip vertically
            cv::flip(pboImage->get(), pboOutput->get(), 0);
        }

        // back to conventional pixel operation
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
#endif
}

GLImpl::~GLImpl() {
    glDeleteBuffers(2, pboIds);
    
    // Cleanup
    if OPENGL3_CONDITION
        ImGui_ImplOpenGL3_Shutdown();
#if !defined(__EMSCRIPTEN__)
    else
        ImGui_ImplOpenGL2_Shutdown();
#endif
    
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}

void GLImpl::enable_readback() {
    
}

void GLImpl::disable_readback() {
    
}

const Image::UPtr& GLImpl::current_frame_buffer() {
    return pboOutput;
}

void GLImpl::check_thread_id(int line, const char* file) const {
#ifndef NDEBUG
    if(std::this_thread::get_id() != _update_thread)
        throw U_EXCEPTION("Wrong thread in '",file,"' line ",line,".");
#endif
}

static std::vector<uchar> empty;

void GLImpl::toggle_full_screen() {
    GLFWmonitor *_monitor = nullptr;

    static int count;
    static auto monitors = glfwGetMonitors(&count);

    int wx, wy, wh, ww;

    // backup window position and window size
    glfwGetWindowPos(window_handle(), &wx, &wy);
    glfwGetWindowSize(window_handle(), &ww, &wh);

    Vec2 center = Vec2(wx, wy) + Size2(ww, wh) * 0.5;

#if !defined(__EMSCRIPTEN__)
    for (int i = 0; i < count; ++i) {
        int x, y, w, h;
        glfwGetMonitorWorkarea(monitors[i], &x, &y, &w, &h);
        if (Bounds(x, y, w, h).contains(center)) {
            _monitor = monitors[i];
        }
    }
#endif

    // get resolution of monitor
    const GLFWvidmode * mode = glfwGetVideoMode(_monitor);
    if (_monitor == nullptr)
        _monitor = glfwGetPrimaryMonitor();
    
    if ( fullscreen )
    {
        // backup window position and window size
        glfwGetWindowPos( window_handle(), &_wndPos[0], &_wndPos[1] );
        glfwGetWindowSize( window_handle(), &_wndSize[0], &_wndSize[1] );

        // switch to full screen
        glfwSetWindowSize(window_handle(), mode->width, mode->height);
        glfwSetWindowPos(window_handle(), 0, 0);
        glfwSetWindowMonitor(window_handle(), _monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        //glfwSetWindowMonitor( window_handle(), _monitor, 0, 0, mode->width, mode->height, 0 );
        fullscreen = false;
    }
    else
    {
        // restore last window size and position
        glfwSetWindowMonitor( window_handle(), nullptr,  _wndPos[0], _wndPos[1], _wndSize[0], _wndSize[1], mode->refreshRate );
        fullscreen = true;
        
    }
}

TexturePtr GLImpl::texture(const Image * ptr) {
    GLIMPL_CHECK_THREAD_ID();
    
    // Turn the RGBA pixel data into an OpenGL texture:
    GLuint my_opengl_texture;
    glGenTextures(1, &my_opengl_texture);
    if(my_opengl_texture == 0)
        throw U_EXCEPTION("Cannot create texture of dimensions ",ptr->cols,"x", ptr->rows);
    glBindTexture(GL_TEXTURE_2D, my_opengl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (ptr->dims == 3 || ptr->dims > 4) {
        FormatExcept("Cannot load pixel store alignment of ", ptr->dims);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    } else
        glPixelStorei(GL_UNPACK_ALIGNMENT, ptr->dims);
    
#if !CMN_USE_OPENGL2
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#endif
    
    GLint output_type = GL_RGBA8;
    GLenum input_type = GL_RGBA;
    
#ifdef __EMSCRIPTEN__
    output_type = GL_RGBA;
    if (ptr->dims == 1) {
        input_type = output_type = GL_LUMINANCE;
    }
    else if (ptr->dims == 2) {
        input_type = output_type = GL_LUMINANCE_ALPHA;
    }
    else {
        if (ptr->dims != 4)
            throw U_EXCEPTION("Channels ", ptr->dims, " was expected to be RGBA format.");
    }
#else
    if OPENGL3_CONDITION {
        if(ptr->dims == 1) {
            output_type = GL_RED;
            input_type = GL_RED;
            //GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
            //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }
        if(ptr->dims == 2) {
            //output_type = GL_RG8;
            input_type = GL_RG;

            GLint swizzleMask[] = {GL_RED, GL_ZERO, GL_ZERO, GL_GREEN};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }

    }
    else {
        output_type = GL_RGBA;

        if (ptr->dims == 1) {
            output_type = GL_LUMINANCE;
            input_type = GL_LUMINANCE;
        }
        if (ptr->dims == 2) {
            output_type = GL_LUMINANCE_ALPHA;
            input_type = GL_LUMINANCE_ALPHA;
        }
    }
#endif
    
#if defined(__EMSCRIPTEN__)
    auto width = ptr->cols, height = ptr->rows;
#else
    auto width = next_pow2(sign_cast<uint64_t>(ptr->cols)), height = next_pow2(sign_cast<uint64_t>(ptr->rows));
#endif
    auto capacity = size_t(ptr->dims) * size_t(width) * size_t(height);
    if (empty.size() < capacity)
        empty.resize(capacity, 0);

#if defined(__EMSCRIPTEN__)
    glTexImage2D(GL_TEXTURE_2D, 0, output_type, width, height, 0, input_type, GL_UNSIGNED_BYTE, ptr->data()); checkGLError();
#else
    glTexImage2D(GL_TEXTURE_2D, 0, output_type, width, height, 0, input_type, GL_UNSIGNED_BYTE, empty.data()); checkGLError();

    //print("updating texture ",ptr->cols, "x", ptr->rows, " -> input:", input_type, " size:", ptr->size(), " - dims:", ptr->dimensions(), " capacity:", capacity, " (",width,"x",height,")");
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0, (GLsizei)ptr->cols, (GLsizei)ptr->rows, input_type, GL_UNSIGNED_BYTE, ptr->data()); checkGLError();
#endif
    glBindTexture(GL_TEXTURE_2D, 0); checkGLError();
    return std::make_unique<PlatformTexture>(
        new ImTextureID_t{ (uint64_t)my_opengl_texture,
#if !defined(__EMSCRIPTEN__)
            uint8_t(ptr->dims)
#else
            uint8_t(saturate(width))
#endif
        },
        [texture_updates = _texture_updates, mutex_share = texture_mutex](void ** ptr) {
            std::lock_guard guard(*mutex_share);
            texture_updates->emplace_back([object = (ImTextureID_t*)*ptr](){
                //GLIMPL_CHECK_THREAD_ID();
                
                GLuint _id = (GLuint) object->texture_id;
                
                glBindTexture(GL_TEXTURE_2D, _id); checkGLError();
                glDeleteTextures(1, &_id); checkGLError();
                glBindTexture(GL_TEXTURE_2D, 0); checkGLError();
                
                delete object;
            });
            
            *ptr = nullptr;
        },
        static_cast<uint>(width), static_cast<uint>(height),
        ptr->cols, ptr->rows
    );
}

void GLImpl::clear_texture(TexturePtr&&) {
    /*GLIMPL_CHECK_THREAD_ID();
    
    auto object = (ImTextureID_t*)id_->ptr;
    GLuint _id = (GLuint) object->texture_id;
    
    glBindTexture(GL_TEXTURE_2D, _id);
    glDeleteTextures(1, &_id);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    delete object;*/
}

void GLImpl::bind_texture(const PlatformTexture& id_) {
    GLIMPL_CHECK_THREAD_ID();
    
    auto object = (ImTextureID_t*)id_.ptr;
    GLuint _id = (GLuint) object->texture_id;
    
    glBindTexture(GL_TEXTURE_2D, _id);
}

void GLImpl::update_texture(PlatformTexture& id_, const Image *ptr) {
    GLIMPL_CHECK_THREAD_ID();
    
    auto object = (ImTextureID_t*)id_.ptr;
    GLuint _id = (GLuint) object->texture_id;
    
    //if(object->greyscale != (ptr->dims == 2))
    //    throw U_EXCEPTION("Texture has not been allocated for number of color channels in Image (",ptr->dims,") != texture (",object->greyscale ? 1 : 4,")");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _id);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (ptr->dims == 3 || ptr->dims > 4) {
        FormatExcept("Cannot load pixel store alignment of ", ptr->dims);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }
    else
        glPixelStorei(GL_UNPACK_ALIGNMENT, ptr->dims);

    checkGLError();
    
    GLint output_type = GL_RGBA8;
    GLenum input_type = GL_RGBA;

#ifdef __EMSCRIPTEN__
    output_type = GL_RGBA;
    if (ptr->dims == 1) {
        input_type = output_type = GL_LUMINANCE;
    }
    else if (ptr->dims == 2) {
        input_type = output_type = GL_LUMINANCE_ALPHA;
    }
    else {
        if (ptr->dims != 4)
            throw U_EXCEPTION("Channels ", ptr->dims, " was expected to be RGBA format.");
    }
#else
    if OPENGL3_CONDITION{
        if (ptr->dims == 1) {
            output_type = GL_RED;
            input_type = GL_RED;
            //GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
            //glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }
        if (ptr->dims == 2) {
            //output_type = GL_RG8;
            input_type = GL_RG;

            GLint swizzleMask[] = {GL_RED, GL_ZERO, GL_ZERO, GL_GREEN};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        }

    }
    else {
        output_type = GL_RGBA;

        if (ptr->dims == 1) {
            //output_type = GL_LUMINANCE;
            input_type = GL_LUMINANCE;
        }
        if (ptr->dims == 2) {
            //output_type = GL_LUMINANCE_ALPHA;
            input_type = GL_LUMINANCE_ALPHA;
        }
    }
#endif

#if !defined(__EMSCRIPTEN__)
    auto capacity = size_t(ptr->dims) * size_t(id_.width) * size_t(id_.height);
    if (empty.size() < capacity)
        empty.resize(capacity, 0);
    
    if (ptr->cols != (uint)id_.width || ptr->rows != (uint)id_.height) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)ptr->cols, 0, GLint(id_.width) - GLint(ptr->cols), id_.height, input_type, GL_UNSIGNED_BYTE, empty.data()); checkGLError();
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)ptr->rows, (GLint)ptr->cols, GLint(id_.height) - GLint(ptr->rows), input_type, GL_UNSIGNED_BYTE, empty.data()); checkGLError();
        //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, id_.width, id_.height, input_type, GL_UNSIGNED_BYTE, empty.data());
    }
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0, (GLint)ptr->cols, (GLint)ptr->rows, input_type, GL_UNSIGNED_BYTE, ptr->data()); checkGLError();
#else
    glTexImage2D(GL_TEXTURE_2D, 0, output_type, (GLint)ptr->cols, (GLint)ptr->rows, 0, input_type, GL_UNSIGNED_BYTE, ptr->data()); checkGLError();
#endif
    glBindTexture(GL_TEXTURE_2D, 0); checkGLError();
    
    id_.image_width = int(ptr->cols);
    id_.image_height = int(ptr->rows);
}

void GLImpl::set_title(std::string title) {
    glfwSetWindowTitle(window, title.c_str());
}

}
#endif

