#include <commons.pc.h>
#include "MetalImpl.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_metal.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <misc/Timer.h>
#include <misc/GlobalSettings.h>

#include <Availability.h>

#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101500
#define NO_SWIZZLE_DIZZLE
#endif
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101300
#define NO_ALLOWS_NEXT_DRAWABLE
#endif
#endif

#import <objc/runtime.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#define GLIMPL_CHECK_THREAD_ID() check_thread_id( __LINE__ , __FILE__ )

namespace cmn::gui {
struct MetalData {
    id <MTLDevice> device;
    id <MTLCommandQueue> commandQueue;
    CAMetalLayer *layer;
    MTLRenderPassDescriptor *renderPassDescriptor;
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
    MTLComputePassDescriptor *computePassDescriptor;
    MTLTextureDescriptor *computeTextureDescriptor;
    MTLTextureDescriptor *binaryTextureDescriptor;
    id<MTLTexture> stencilTexture;
    id<MTLTexture> maskTexture;
    id<MTLTexture> binaryTexture, testTexture;
#endif
};
namespace metal {
gui::MetalImpl * current_instance = nullptr;
}
}

@interface GLFWCustomDelegate : NSObject
+ (void)load; // load is called before even main() is run (as part of objc class registration)
@end

// part of your application

bool startup_kind_of_done = false;
std::string startup_file_to_load = "";

extern "C"{
    bool forward_load_message(const std::vector<cmn::file::Path>& paths){
        auto str = cmn::Meta::toStr(paths);
        NSString* string = [NSString stringWithCString:str.c_str() encoding:NSASCIIStringEncoding];
        cmn::Print("Open file: ", str.c_str());
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            auto cstr = [string cStringUsingEncoding:NSASCIIStringEncoding];
            auto paths = cmn::Meta::fromStr<std::vector<cmn::file::Path>>(cstr);
            for(auto it = paths.begin(); it != paths.end(); ) {
                if(!it->exists()) {
                    it = paths.erase(it);
                } else
                    ++it;
            }
            
            if(!paths.empty() && cmn::gui::metal::current_instance) {
                if(!cmn::gui::metal::current_instance->open_files(paths)) {
                    //! Do not throw message box.
                    //gui::metal::current_instance->message("Cannot open "+std::string(cstr)+".");
                }
            }
                
        });
        
        return false;
    }
}

@implementation GLFWCustomDelegate

+ (void)load{


    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        {
            Class target_c = [GLFWCustomDelegate class];
            Method originalMethod = class_getInstanceMethod(objc_getClass("GLFWApplicationDelegate"), @selector(application:openFiles:));
            Method swizzledMethod = class_getInstanceMethod(target_c, @selector(swz_application:openFiles:));

            BOOL didAddMethod =
            class_addMethod(objc_getClass("GLFWApplicationDelegate"),
                            @selector(application:openFiles:),
                            method_getImplementation(swizzledMethod),
                            method_getTypeEncoding(swizzledMethod));

            if (didAddMethod) {
                class_replaceMethod(objc_getClass("GLFWApplicationDelegate"),
                                    @selector(swz_application:openFiles:),
                                    method_getImplementation(originalMethod),
                                    method_getTypeEncoding(originalMethod));
            } else {
                method_exchangeImplementations(originalMethod, swizzledMethod);
            }
        }
        
        Class target_c = [GLFWCustomDelegate class];
        Method originalMethod = class_getInstanceMethod(objc_getClass("GLFWApplicationDelegate"), @selector(application:openFile:));
        Method swizzledMethod = class_getInstanceMethod(target_c, @selector(swz_application:openFile:));

        BOOL didAddMethod =
        class_addMethod(objc_getClass("GLFWApplicationDelegate"),
                        @selector(application:openFile:),
                        method_getImplementation(swizzledMethod),
                        method_getTypeEncoding(swizzledMethod));

        if (didAddMethod) {
            class_replaceMethod(objc_getClass("GLFWApplicationDelegate"),
                                @selector(swz_application:openFile:),
                                method_getImplementation(originalMethod),
                                method_getTypeEncoding(originalMethod));
        } else {
            method_exchangeImplementations(originalMethod, swizzledMethod);
        }
    });
 
    cmn::gui::MetalImpl::SetDockIcon("AlterIcons");
}

- (BOOL)swz_application:(NSApplication *)sender openFile:(NSString *)filename{
    return forward_load_message({filename.UTF8String});
}

- (void)swz_application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames{
    std::vector<cmn::file::Path> paths;
    for(size_t i = 0; i < filenames.count; ++i)
        paths.push_back(filenames[i].UTF8String);
    forward_load_message(paths);
}

@end

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

namespace cmn::gui {

void MetalImpl::SetDockIcon(const char* icnsNameWithoutExtension)
{
    @autoreleasepool
    {
        // Locate AlterIcons.icns that you added to your bundle’s Resources
        NSString* iconPath = [[NSBundle mainBundle]
                              pathForResource:[NSString stringWithUTF8String:icnsNameWithoutExtension]
                              ofType:@"icns"];
        if (!iconPath) return;

        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        if (!icon) return;

        [NSApp setApplicationIconImage:icon];
    }
}

static dispatch_semaphore_t& frameBoundarySemaphore() {
    static dispatch_semaphore_t sem;
    return sem;
}
static auto& shutdown_mutex() {
    static auto _shutdown_mutex = new LOGGED_MUTEX("MetalImpl::shutdown_mutex");
    return *_shutdown_mutex;
}

void MetalImpl::check_thread_id(int line, const char* file) const {
#ifndef NDEBUG
    if(std::this_thread::get_id() != _update_thread)
        throw U_EXCEPTION("Wrong thread in '",file,"' line ",line,".");
#else
    UNUSED(line);
    UNUSED(file);
#endif
}

    MetalImpl::MetalImpl(std::function<void()> draw, std::function<bool()> new_frame_fn)
        : draw_function(draw), new_frame_fn(new_frame_fn), _data(new MetalData)
    {
        gui::metal::current_instance = this;
        frameBoundarySemaphore() = dispatch_semaphore_create(1);
        _gui_macos_blur = GlobalSettings::has("gui_macos_blur") ? SETTING(gui_macos_blur).value<bool>() : false;
        
        GlobalSettings::map().register_callbacks({"gui_macos_blur"}, [this](std::string_view name) {
            _gui_macos_blur = GlobalSettings::map().at(name).value<bool>();
        });
        //dispatch_semaphore_signal(_frameBoundarySemaphore);
    }

    MetalImpl::~MetalImpl() {
        GLIMPL_CHECK_THREAD_ID();
        
        GlobalSettings::map().unregister_callbacks(std::move(_callback));
        
        auto guard = LOGGED_LOCK(shutdown_mutex());
        if(dispatch_semaphore_wait(frameBoundarySemaphore(), dispatch_time(DISPATCH_TIME_NOW, 500000000lu)) != 0)
        {
            FormatError("Semaphore did not receive a signal and had to time out.");
        }
        
        if(gui::metal::current_instance == this)
            gui::metal::current_instance = nullptr;
        
        // Cleanup
        ImGui_ImplMetal_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window);
        glfwTerminate();
    }

bool MetalImpl::open_files(const std::vector<file::Path> &paths) {
    if(_fn_open_files)
        return _fn_open_files(paths);
    return false;
}

    void MetalImpl::init()
    {
        _update_thread = std::this_thread::get_id();
        
        // Setup Dear ImGui binding
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
        
        // Setup style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();
        
        // Setup window
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            throw U_EXCEPTION("[METAL] Cannot init GLFW.");
        
        SetDockIcon("AlterIcons");
    }

    void MetalImpl::post_init() {
        ImGui_ImplMetal_Init(_data->device);
        
        _data->commandQueue = [_data->device newCommandQueue];
    }
    
    void MetalImpl::create_window(const char* title, int width, int height) {
        // Create window with graphics context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, title, NULL, NULL);
        if (window == NULL)
            throw U_EXCEPTION("[METAL] Cannot create GLFW window.");
        
        
        _data->device = MTLCreateSystemDefaultDevice();
        //_data->commandQueue = [_data->device newCommandQueue];
        
        NSWindow *nswin = glfwGetCocoaWindow(window);
        _data->layer = [CAMetalLayer layer];
        _data->layer.device = _data->device;
        _data->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
#ifndef NO_ALLOWS_NEXT_DRAWABLE
        _data->layer.allowsNextDrawableTimeout = YES;
        _data->layer.displaySyncEnabled = YES;
#endif
        _data->layer.framebufferOnly = NO;
        nswin.contentView.layer = _data->layer;
        nswin.contentView.wantsLayer = YES;
        
        _data->renderPassDescriptor = [MTLRenderPassDescriptor new];
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
        _data->computePassDescriptor = [MTLComputePassDescriptor new];
        _data->computeTextureDescriptor = nullptr;
        _data->binaryTextureDescriptor = nullptr;
#endif
        
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        [nswin setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    }

    void MetalImpl::toggle_full_screen() {
        NSWindow *nswin = glfwGetCocoaWindow(window);
        [nswin toggleFullScreen:[NSApplication sharedApplication]];
    }

void MetalImpl::message(const std::string &msg) const {
    NSWindow *win = glfwGetCocoaWindow(gui::metal::current_instance->window_handle());
    NSAlert * alert = [[[NSAlert alloc] init] autorelease];
    [alert setMessageText:[NSString stringWithCString:msg.c_str() encoding:NSASCIIStringEncoding]];
    [alert beginSheetModalForWindow:win completionHandler:^(NSModalResponse){}];
}

std::mutex framebuffer_lock;
id mutexObject = [[NSObject alloc] init];

    LoopStatus MetalImpl::update_loop(const CrossPlatform::custom_function_t& custom_loop) {
        GLIMPL_CHECK_THREAD_ID();
        
        auto guard = LOGGED_LOCK(shutdown_mutex());
        LoopStatus status = LoopStatus::IDLE;
        
        if(custom_loop) {
            if(!custom_loop())
                return LoopStatus::END;
        }
        
        dispatch_semaphore_wait(frameBoundarySemaphore(), DISPATCH_TIME_FOREVER);
        
        ++frame_index;
        
        glfwPollEvents();
        if(glfwWindowShouldClose(window)) {
            dispatch_semaphore_signal(frameBoundarySemaphore());
            return LoopStatus::END;
        }
        
        if(new_frame_fn()) {
            //@synchronized (mutexObject) {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
            if(width <= 0 || height <= 0) {
                if(width <= 0)
                    width = 640;
                if(height <= 0)
                    height = 480;
            }
            
            assert(width > 0 && height > 0);
                
                {
                    std::unique_lock guard(framebuffer_lock);
                    if(_frame_capture_enabled) {
                    } else if(_current_framebuffer)
                        _current_framebuffer = nullptr;
                }
                
                @autoreleasepool {
                    
                    _data->layer.drawableSize = CGSizeMake(width, height);
                    
                    id<CAMetalDrawable> drawable = [_data->layer nextDrawable];
                    {
                        std::unique_lock guard(framebuffer_lock);
                        if(_frame_capture_enabled) {
                            if(!_current_framebuffer) {
                                _current_framebuffer = Image::Make(height, width, 4);
                            }
                        } else if(_current_framebuffer)
                            _current_framebuffer = nullptr;
                    }
                    
                    id<MTLCommandBuffer> commandBuffer = [_data->commandQueue commandBuffer];
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
                    if(!_data->computeTextureDescriptor || (int)_data->computeTextureDescriptor.width != width || (int)_data->computeTextureDescriptor.height != height) {
                        MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
                        
                        // Set the pixel dimensions of the texture
                        textureDescriptor.width = width;
                        textureDescriptor.height = height;
                        textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
                        textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                        
                        _data->computeTextureDescriptor = textureDescriptor;
                        textureDescriptor = [[MTLTextureDescriptor alloc] init];
                        
                        // Set the pixel dimensions of the texture
                        textureDescriptor.width = width;
                        textureDescriptor.height = height;
                        textureDescriptor.pixelFormat = MTLPixelFormatR8Unorm;
                        textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
                        
                        _data->binaryTextureDescriptor = textureDescriptor;
                        _data->maskTexture = nil;
                        _data->stencilTexture = nil;
                        _data->binaryTexture = nil;
                        _data->testTexture = nil;
                    }
                    
                    // Create the texture from the device by using the descriptor
                    if(!_data->maskTexture)
                        _data->maskTexture = [commandBuffer.device newTextureWithDescriptor:_data->computeTextureDescriptor];
                    if(!_data->testTexture)
                        _data->testTexture = [commandBuffer.device newTextureWithDescriptor:_data->computeTextureDescriptor];
                    if(!_data->binaryTexture)
                        _data->binaryTexture = [commandBuffer.device newTextureWithDescriptor:_data->binaryTextureDescriptor];
#endif
                    _data->renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(_clear_color[0] / 255.f, _clear_color[1] / 255.f, _clear_color[2] / 255.f, _clear_color[3] / 255.f);
                    _data->renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
                    _data->renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                    _data->renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
                    _data->renderPassDescriptor.colorAttachments[1].texture = _data->maskTexture;
                    _data->renderPassDescriptor.colorAttachments[1].loadAction = MTLLoadActionClear;
                    _data->renderPassDescriptor.colorAttachments[1].clearColor = MTLClearColorMake(0, 0, 0, 0);
                    _data->renderPassDescriptor.colorAttachments[1].storeAction = MTLStoreActionStore;
                    
                    _data->renderPassDescriptor.colorAttachments[2].texture = _data->binaryTexture;
                    _data->renderPassDescriptor.colorAttachments[2].loadAction = MTLLoadActionClear;
                    _data->renderPassDescriptor.colorAttachments[2].clearColor = MTLClearColorMake(0, 0, 0, 0);
                    _data->renderPassDescriptor.colorAttachments[2].storeAction = MTLStoreActionStore;
#endif
                    id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_data->renderPassDescriptor];
                    
                    [renderEncoder pushDebugGroup:@"TRex"];
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
                    uint8_t buffer = _gui_macos_blur;
                    [renderEncoder setVertexBytes:&buffer length:sizeof(buffer) atIndex:2];
#endif
                    
                    // Start the Dear ImGui frame
                    ImGui_ImplMetal_NewFrame(_data->renderPassDescriptor);
                    ImGui_ImplGlfw_NewFrame();
                    ImGui::NewFrame();
                    
                    draw_function();
                    
                    // Rendering
                    ImGui::Render();
                    
                    {
                        std::lock_guard guard(_texture_mutex);
                        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);
                    }
                    
                    [renderEncoder popDebugGroup];
                    [renderEncoder endEncoding];
                    
#ifdef TREX_ENABLE_EXPERIMENTAL_BLUR
                    if(_gui_macos_blur) {
                        static id<MTLLibrary> library;
                        
                        //static MPSImageGaussianBlur* kernel = [[MPSImageGaussianBlur alloc] initWithDevice:_data->device sigma:7.0];
                        //[kernel encodeToCommandBuffer:commandBuffer inPlaceTexture:&_data->maskTexture fallbackCopyAllocator:nil];
                        
                        id<MTLComputeCommandEncoder> computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:_data->computePassDescriptor];
                        
                        static id<MTLComputePipelineState> computeState;
                        
                        if(!library) {
                            NSString *shaderSource = @"#include <metal_stdlib>\n"
                            "using namespace metal;\n"
                            "\n"
                            "struct Uniforms {\n"
                            "    float center_x;\n"
                            "    float center_y;\n"
                            "};\n"
                            "kernel void computeShader(\n"
                            "    texture2d<half, access::read> source [[ texture(0) ]],\n"
                            "    texture2d<half, access::read> mask [[ texture(1) ]],\n"
                            "    texture2d<half, access::write> dest [[ texture(2) ]],\n"
                            "    texture2d<half, access::read> dtest [[ texture(3) ]],\n"
                            "    uint2 gid [[ thread_position_in_grid ]])\n"
                            "    {\n"
                            "        half4 source_color = source.read(gid);\n"
                            "        half4 mask_color = mask.read(gid) * min(1.0, (1.0 - dtest.read(gid).r) * 2.0);\n"
                            "        half4 result_color = (1 - mask_color.a) * source_color + (mask_color.a) * mask_color;\n"
                            "        result_color.a = source_color.a;\n"
                            "        \n"
                            "        dest.write(result_color, gid);\n"
                            "}\n"
                            "kernel void customBlur(\n"
                            "    texture2d<half, access::sample> source [[ texture(0) ]],\n"
                            "    texture2d<half, access::write> dest [[ texture(1) ]],\n"
                            "    uint2 gid [[ thread_position_in_grid ]],\n"
                            "    constant Uniforms &uniforms [[buffer(0)]]\n"
                            ")\n"
                            "    {\n"
                            "    constexpr sampler linearSampler(coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear);\n"
                            "        const float w = source.get_width(), h = source.get_height();\n"
                            "        constexpr float max_kernel_size = 100.0;\n"
                            "        const float x = (float(gid.x) / w) - uniforms.center_x;\n"
                            "        const float y = (float(gid.y) / h) - uniforms.center_y;\n"
                            "        const float d = sqrt(x * x + y * y);\n"
                            "        constexpr int kernel_size = 3;\n"
                            "        const float step = max_kernel_size * d * d / (kernel_size * 2);"
                            "        const float2 dim(w, h);"
                            "    float4 result_color = float4(source.sample(linearSampler, float2(gid) / dim));\n"
                            //"    float N = (kernel_size + 1) * (kernel_size + 1) * 4 + 1;"
                            "    float N = 1.0;\n"
                            "    float2 coord = float2(gid) / dim;"
                            "    for (int i=1; i<kernel_size + 1; ++i) {\n"
                            "for(int j=1; j<kernel_size + 1; ++j) {\n"
                            "result_color += float4(source.sample(linearSampler, coord + float2( i, j) / dim * step));\n"
                            "result_color += float4(source.sample(linearSampler, coord + float2( i,-j) / dim * step));\n"
                            "result_color += float4(source.sample(linearSampler, coord + float2(-i, j) / dim * step));\n"
                            "result_color += float4(source.sample(linearSampler, coord + float2(-i,-j) / dim * step));\n"
                            "N += 4.0;\n"
                            "}\n"
                            "    }\n"
                            "        result_color /= N;"
                            "        dest.write(half4(result_color), gid);\n"
                            "   }\n";
                            
                            NSError *error;
                            library = [commandBuffer.device newLibraryWithSource:shaderSource options:nil error:&error];
                            if (library == nil)
                            {
                                NSLog(@"Error: failed to create Metal library: %@", error);
                            }
                        }
                        
                        MTLSize threadGroupSize = MTLSizeMake(8, 8, 1);
                        
                        id<MTLFunction> blurFunction = [library newFunctionWithName:@"customBlur"];
                        computeState = [commandBuffer.device newComputePipelineStateWithFunction:blurFunction error:nil];
                        
                        [computeEncoder setComputePipelineState:computeState];
                        id<MTLBuffer> mbuffer = [_data->device newBufferWithBytes:center length:sizeof(center) options:MTLResourceCPUCacheModeDefaultCache];
                        [computeEncoder setBuffer:mbuffer offset:0 atIndex:0];
                        [computeEncoder setTexture:_data->maskTexture atIndex:0];
                        
                        [computeEncoder setTexture:_data->testTexture atIndex:1];
                        //[computeEncoder dispatchThreads:MTLSizeMake(width, height, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                        [computeEncoder dispatchThreadgroups:MTLSizeMake(width/threadGroupSize.width+1, height/threadGroupSize.height+1, 1) threadsPerThreadgroup:threadGroupSize];
                        [computeEncoder endEncoding];
                        
                        computeEncoder = [commandBuffer computeCommandEncoderWithDescriptor:_data->computePassDescriptor];
                        id<MTLFunction> computeFunction = [library newFunctionWithName:@"computeShader"];
                        computeState = [commandBuffer.device newComputePipelineStateWithFunction:computeFunction error:nil];
                        
                        [computeEncoder setComputePipelineState:computeState];
                        [computeEncoder setTexture:drawable.texture atIndex:0];
                        [computeEncoder setTexture:_data->testTexture atIndex:1];
                        [computeEncoder setTexture:drawable.texture atIndex:2];
                        [computeEncoder setTexture:_data->binaryTexture atIndex:3];
                        
                        [computeEncoder dispatchThreadgroups:
                         MTLSizeMake(width/threadGroupSize.width+1, height/threadGroupSize.height+1, 1) threadsPerThreadgroup:threadGroupSize];
                        [computeEncoder endEncoding];
                    }
#endif
                    
                    [commandBuffer presentDrawable:drawable];
                    
                    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
                        try {
                            if(_frame_capture_enabled) {
                                //[commandBuffer waitUntilCompleted];
                                std::unique_lock guard(framebuffer_lock);
                                if(not _current_framebuffer || _current_framebuffer->rows != drawable.texture.height || _current_framebuffer->cols != drawable.texture.width)
                                    _current_framebuffer = Image::Make(drawable.texture.height, drawable.texture.width, 4);
                                
                                [drawable.texture getBytes:_current_framebuffer->data() bytesPerRow:_current_framebuffer->dims * _current_framebuffer->cols fromRegion:MTLRegionMake2D(0, 0, _current_framebuffer->cols, _current_framebuffer->rows) mipmapLevel:0];
                                
                                if(_frame_buffer_receiver)
                                    _frame_buffer_receiver(std::move(_current_framebuffer));
                            }
                            
                            {
                                auto guard = LOGGED_LOCK(_texture_mutex);
                                for(auto ptr : _delete_textures) {
                                    id<MTLTexture> texture = (__bridge id<MTLTexture>)ptr;
                                    [texture release];
                                }
                                _delete_textures.clear();
                            }
                        } catch(const std::exception& ex) {
                            FormatError("Exception in semaphore context: ", ex.what());
                        }
                        
                        dispatch_semaphore_signal(frameBoundarySemaphore());
                    }];
                    
                    [commandBuffer commit];
                }
                
            //}
            ++_draw_calls;
            status = LoopStatus::UPDATED;
            
        } else
            dispatch_semaphore_signal(frameBoundarySemaphore());
        
        /*if(_draw_timer.elapsed() >= 1) {
            Print(_draw_calls," draw_calls / s");
            _draw_calls = 0;
            _draw_timer.reset();
        }*/
        
        return status;
    }

void MetalImpl::set_frame_buffer_receiver(std::function<void (Image::Ptr &&)> fn) {
    std::unique_lock guard(framebuffer_lock);
    _frame_buffer_receiver = fn;
}
    
    /*void MetalImpl::loop(CrossPlatform::custom_function_t custom_loop) {
        LoopStatus status = LoopStatus::IDLE;
        
        // Main loop
        while (status != LoopStatus::END)
        {
            if(!custom_loop())
                break;
            
            status = update_loop();
            if(status != gui::LoopStatus::UPDATED)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }*/

    TexturePtr MetalImpl::texture(const Image * ptr) {
        GLIMPL_CHECK_THREAD_ID();
        
        uint width = narrow_cast<uint>(next_pow2(sign_cast<uint64_t>(ptr->cols)));
        uint height = narrow_cast<uint>(next_pow2(sign_cast<uint64_t>(ptr->rows)));
        
        MTLPixelFormat input_format;
        if(ptr->dims == 4) {
            input_format = MTLPixelFormatRGBA8Unorm;
        } else if(ptr->dims == 1) {
            input_format = MTLPixelFormatR8Unorm;
        } else if(ptr->dims == 2) {
            input_format = MTLPixelFormatRG8Unorm;
        } else {
            throw U_EXCEPTION("Unsupported number of dimensions: ", ptr->dims, " in image ", *ptr);
        }
        
        assert(width > 0 && height > 0);
        MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:input_format width:width height:height mipmapped:NO];
        textureDescriptor.usage = MTLTextureUsageShaderRead;
#if defined(__arm64__) || defined(__aarch64__)
        textureDescriptor.storageMode = MTLStorageModeShared;
#else
        textureDescriptor.storageMode = MTLStorageModeManaged;
#endif

#ifndef NO_SWIZZLE_DIZZLE
        if (@available(macOS 10.15, *)) {
            if(ptr->dims != 4) {
                MTLTextureSwizzleChannels swizzle;
                
                if(ptr->dims == 1) {
                    swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleOne);
                } else if(ptr->dims == 2) {
                    swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleGreen);
                } else
                    throw U_EXCEPTION("Unknown texture format with ",ptr->dims," channels.");
                textureDescriptor.swizzle = swizzle;
                //texture = [texture newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm textureType:MTLTextureType2D levels:NSMakeRange(0, 0) slices:NSMakeRange(0, 0) swizzle:swizzle];
            }
        }
#endif
        
        id <MTLTexture> texture = [_data->device newTextureWithDescriptor:textureDescriptor];
        [texture replaceRegion:MTLRegionMake2D(0, 0, ptr->cols, ptr->rows) mipmapLevel:0 withBytes:ptr->data() bytesPerRow:ptr->cols * ptr->dims];
        
        return std::unique_ptr<PlatformTexture>(new PlatformTexture{(__bridge void*)texture, [this](void** ptr){
            std::lock_guard<std::mutex> guard(_texture_mutex);
            _delete_textures.emplace_back(*ptr);
            
            *ptr = nullptr;
            //id<MTLTexture> texture = (__bridge id<MTLTexture>)ptr;
            //[texture release];
        }, width, height, ptr->cols, ptr->rows});
    }

    void MetalImpl::clear_texture(TexturePtr&&) {
        /*std::lock_guard<std::mutex> guard(_texture_mutex);
        id<MTLTexture> texture = (__bridge id<MTLTexture>)tex->ptr;
        [texture release];*/
    }

    void MetalImpl::bind_texture(const PlatformTexture&) {
        
    }

    template <typename T, std::size_t N, T DefaultValue, std::size_t... I>
    constexpr std::array<T, N> create_filled_array(std::index_sequence<I...>) {
        // Use fold expression to initialize the array.
        return {{(I, DefaultValue)...}};
    }

    template <typename T, std::size_t N, T DefaultValue>
    constexpr std::array<T, N> create_filled_array() {
        return create_filled_array<T, N, DefaultValue>(std::make_index_sequence<N>{});
    }

    void MetalImpl::update_texture(PlatformTexture& tex, const Image * ptr) {
        GLIMPL_CHECK_THREAD_ID();
        
        id<MTLTexture> texture = (__bridge id<MTLTexture>)tex.ptr;
        
        MTLRegion region = {
            { 0, 0, 0 },                   // MTLOrigin
            {ptr->cols, ptr->rows, 1} // MTLSize
        };
        
        /*bool d = rand() > RAND_MAX / 2;
        if(not d)*/ {
            //static Timing timing("mostly cpu", 0.1);
            //TakeTiming take(timing);
            /*if(ptr->cols < tex.image_width
               || ptr->rows < tex.image_height)
            {
                static constexpr auto zeros = create_filled_array<uchar, 4096, 0>();
                MTLRegion region {
                    { 0, 0, 0 },                   // MTLOrigin
                    { // MTLSize
                        min(max(ptr->cols + 1, tex.image_width + 1), tex.width),
                        min(max(ptr->rows + 1, tex.image_height + 1), tex.height),
                        1
                    }
                };
                
                if(max(region.size.width - ptr->cols, region.size.height - ptr->rows) * ptr->dims < zeros.size())
                {
                    if(region.size.width > ptr->cols) {
                        NSUInteger bytesPerRow = ptr->dims * (region.size.width - ptr->cols);
                        [texture replaceRegion:
                         MTLRegion{
                            { ptr->cols, 0, 0 },
                            { region.size.width - ptr->cols, ptr->rows, 1 }
                        }
                                   mipmapLevel:0
                                     withBytes:zeros.data()
                                   bytesPerRow:bytesPerRow];
                    }
                    
                    if(region.size.height > ptr->rows) {
                        NSUInteger bytesPerRow = ptr->dims * region.size.width;
                        [texture replaceRegion:
                         MTLRegion{
                            { 0, ptr->rows, 0 },
                            { region.size.width, region.size.height - ptr->rows, 1 }
                        }
                                   mipmapLevel:0
                                     withBytes:zeros.data()
                                   bytesPerRow:bytesPerRow];
                    }
                }
            }*/
            
            NSUInteger bytesPerRow = ptr->dims * ptr->cols;
            [texture replaceRegion:region
                       mipmapLevel:0
                         withBytes:ptr->data()
                       bytesPerRow:bytesPerRow];
        }
        
        /*if(d) {
            //static Timing timing("only gpu", 0.1);
            //TakeTiming take(timing);
            if(ptr->cols < tex.image_width
               || ptr->rows < tex.image_height)
            {
                static std::vector<uchar> zeros;
                MTLRegion region = {
                    { 0, 0, 0 },                   // MTLOrigin
                    { // MTLSize
                        min(max(ptr->cols, tex.image_width), tex.width),
                        min(max(ptr->rows, tex.image_height), tex.height),
                        1
                    }
                };
                
                size_t N = region.size.width * region.size.height * ptr->dims + 1;
                if(zeros.size() < N) {
                    zeros.resize(N * 1.25);
                    //zeros.get().setTo(cv::Scalar(0, 0, 0, 0));
                    //Print("resized zeros to ", zeros.size());
                }
                
                //memset(zeros.data(), 0, zeros.size());
                auto data = ptr->data();
                for(uint y = 0; y<ptr->rows; ++y) {
                    memcpy(zeros.data() + ptr->dims * region.size.width * y, data + ptr->cols * ptr->dims * y, ptr->dims * ptr->cols);
                }
                
                if(region.size.width > ptr->cols) {
                    //for(size_t w = ptr->cols; w < region.size.width; ++w) {
                    for(size_t y = 0; y < ptr->rows; ++y) {
                        memset(zeros.data() + ptr->dims * region.size.width * y + ptr->cols * ptr->dims, 0, (region.size.width - ptr->cols) * ptr->dims);
                    }
                    //}
                }
                
                if(region.size.height > ptr->rows) {
                    for(size_t y = ptr->rows; y < region.size.height; ++y)
                        memset(zeros.data() + ptr->dims * region.size.width * y, 0, ptr->dims * region.size.width);
                }
                
                auto z = zeros.data();
                
                [texture replaceRegion:region mipmapLevel:0 withBytes:z bytesPerRow:ptr->dims * region.size.width];
            } else {
                NSUInteger bytesPerRow = ptr->dims * ptr->cols;
                [texture replaceRegion:region
                           mipmapLevel:0
                             withBytes:ptr->data()
                           bytesPerRow:bytesPerRow];
            }
        }*/
        
        tex.image_height = ptr->rows;
        tex.image_width = ptr->cols;
    }

    void MetalImpl::set_title(std::string title) {
        glfwSetWindowTitle(window, title.c_str());
    }

    GLFWwindow* MetalImpl::window_handle() {
        return window;
    }
}
