#pragma once

#include <commons.pc.h>
#include <gui/BrowserProtocol.h>
#include <gui/BrowserTransport.h>
#include <gui/DrawBase.h>
#include <gui/DrawStructure.h>

namespace cmn::gui {
namespace browser {
struct EncodedImage {
    bool png{false};
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> bytes;
};

EncodedImage encode_image(const Image&, int quality);
Size2 logical_viewport_size(std::optional<Size2> native_size,
                            std::optional<Size2> video_size,
                            Float2_t max_browser_width);
uint32_t font_face_style(uint32_t style);
}

struct BrowserBaseConfig {
    std::string title{"TRex"};
    Size2 logical_size{1280, 720};
    browser::TransportConfig transport;
    int image_quality{75};
    file::Path regular_font;
    file::Path italic_font;
    file::Path bold_font;
    file::Path bold_italic_font;
    file::Path monospace_font;
    file::Path monospace_bold_font;
    file::Path symbols_font;
    std::function<void(DrawStructure&, const Event&)> event_callback;
};

class BrowserBase final : public Base {
    struct HeldInput {
        std::unordered_set<std::string> keys;
        std::unordered_set<uint8_t> buttons;
    };

    std::string _title;
    Bounds _window_bounds;
    std::unique_ptr<DrawStructure> _graph;
    browser::BrowserTransport _transport;
    browser::SceneEncoder _scene_encoder;
    std::function<void(DrawStructure&, const Event&)> _event_callback;
    std::atomic<uint64_t> _last_sequence{0};
    std::atomic<size_t> _client_count{0};

    ImFontAtlas _font_atlas;
    std::unordered_map<uint32_t, ImFont*> _fonts;

    std::mutex _input_mutex;
    std::deque<std::pair<browser::BrowserTransport::ClientId, browser::InputPacket>> _discrete_input;
    std::optional<std::pair<browser::BrowserTransport::ClientId, browser::InputPacket>> _latest_pointer_move;
    std::unordered_map<browser::BrowserTransport::ClientId, HeldInput> _held_input;

    std::mutex _image_mutex;
    std::condition_variable _image_condition;
    std::unordered_map<uint32_t, browser::ImageJob> _pending_images;
    std::unordered_map<uint32_t, uint64_t> _active_image_revisions;
    std::thread _image_thread;
    bool _stop_images{false};
    std::atomic<int> _image_quality{75};

    void receive(browser::BrowserTransport::ClientId, const browser::ClientMessage&);
    void disconnected(browser::BrowserTransport::ClientId);
    void release_held_locked(browser::BrowserTransport::ClientId);
    void dispatch(DrawStructure&, const Event&);
    void queue_images(std::vector<browser::ImageJob>);
    void image_loop();
    void load_fonts(const BrowserBaseConfig&);
    ImFont* font_for(uint32_t style) const;

public:
    explicit BrowserBase(BrowserBaseConfig);
    ~BrowserBase() override;

    BrowserBase(const BrowserBase&) = delete;
    BrowserBase& operator=(const BrowserBase&) = delete;

    DrawStructure* graph() const { return _graph.get(); }
    bool start(std::string* error = nullptr);
    void stop();
    void process_input(DrawStructure&);
    size_t client_count() const { return _client_count; }

    LoopStatus update_loop() override;
    void paint(DrawStructure&) override;
    void set_title(std::string) override;
    void set_window_size(Size2) override;
    void set_window_bounds(Bounds) override;
    Bounds get_window_bounds() const override { return _window_bounds; }
    const std::string& title() const override { return _title; }
    Size2 window_dimensions() const override { return _window_bounds.size(); }
    Bounds text_bounds(const std::string&, Drawable*, const Font&) override;
    Float2_t line_spacing(const Font&) override;
    void set_frame_recording(bool) override;
};
}
