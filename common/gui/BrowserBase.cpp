#include "BrowserBase.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace cmn::gui {
namespace {
constexpr Size2 browser_viewport{1280, 720};

bool meaningful_alpha(const cv::Mat& rgba) {
    if(rgba.channels() != 4)
        return false;
    for(int y = 0; y < rgba.rows; ++y) {
        const auto row = rgba.ptr<uint8_t>(y);
        for(int x = 0; x < rgba.cols; ++x) {
            if(row[x * 4 + 3] != 255)
                return true;
        }
    }
    return false;
}

const ImWchar* browser_glyph_ranges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2000, 0x206F,
        0x2070, 0x209F,
        0x20A0, 0x20CF,
        0x2190, 0x27BF,
        0x2B00, 0x2EFF,
        0x3000, 0x303F,
        0x3040, 0x309F,
        0x30A0, 0x30FF,
        0x3400, 0x4DBF,
        0x4E00, 0x9FFF,
        0x1F300, 0x1F900,
        0
    };
    return ranges;
}
}

browser::EncodedImage browser::encode_image(const Image& image, int quality) {
    cv::Mat source = image.get();
    if(source.empty())
        return {};

    EncodedImage result{
        .width = narrow_cast<uint32_t>(source.cols),
        .height = narrow_cast<uint32_t>(source.rows)
    };
    bool png = source.channels() < 3;
    cv::Mat encoded_source;
    switch(source.channels()) {
        case 1:
            encoded_source = source;
            png = true;
            break;
        case 2: {
            encoded_source.create(source.rows, source.cols, CV_8UC4);
            for(int y = 0; y < source.rows; ++y) {
                const auto input = source.ptr<uint8_t>(y);
                auto output = encoded_source.ptr<uint8_t>(y);
                for(int x = 0; x < source.cols; ++x) {
                    output[x * 4 + 0] = input[x * 2 + 0];
                    output[x * 4 + 1] = input[x * 2 + 0];
                    output[x * 4 + 2] = input[x * 2 + 0];
                    output[x * 4 + 3] = input[x * 2 + 1];
                }
            }
            png = true;
            break;
        }
        case 3:
            cv::cvtColor(source, encoded_source, cv::COLOR_RGB2BGR);
            png = false;
            break;
        case 4:
            png = meaningful_alpha(source);
            cv::cvtColor(source, encoded_source, png ? cv::COLOR_RGBA2BGRA : cv::COLOR_RGBA2BGR);
            break;
        default:
            return {};
    }

    const std::vector<int> parameters = png
        ? std::vector<int>{cv::IMWRITE_PNG_COMPRESSION, 3}
        : std::vector<int>{cv::IMWRITE_JPEG_QUALITY, saturate(quality, 0, 100)};
    if(not cv::imencode(png ? ".png" : ".jpg", encoded_source, result.bytes, parameters))
        return {};
    result.png = png;
    return result;
}

Size2 browser::logical_viewport_size(std::optional<Size2> native_size,
                                     std::optional<Size2> video_size,
                                     Float2_t max_browser_width)
{
    if(native_size && native_size->width > 0 && native_size->height > 0)
        return *native_size;
    const auto desired = video_size && video_size->width > 0 && video_size->height > 0
        ? *video_size
        : browser_viewport;
    const auto scale = min(1_F, max_browser_width / desired.width);
    return desired.mul(scale);
}

uint32_t browser::font_face_style(uint32_t style) {
    if(style & Style::Symbols)
        return Style::Symbols;
    if(style & Style::Monospace)
        return Style::Monospace | (style & Style::Bold);
    return style & (Style::Bold | Style::Italic);
}

BrowserBase::BrowserBase(BrowserBaseConfig config)
    : _title(std::move(config.title)),
      _window_bounds(Vec2(), config.logical_size),
      _graph(std::make_unique<DrawStructure>(
          narrow_cast<uint16_t>(min(config.logical_size.width, Float2_t(UINT16_MAX))),
          narrow_cast<uint16_t>(min(config.logical_size.height, Float2_t(UINT16_MAX))))),
      _event_callback(std::move(config.event_callback)),
      _image_quality(saturate(config.image_quality, 0, 100))
{
    load_fonts(config);
    auto transport = std::move(config.transport);
    std::string transport_error;
    const auto started = _transport.start(
        std::move(transport),
        [this](auto client, const auto& message) { receive(client, message); },
        [this](auto client) { disconnected(client); },
        [this](size_t count) {
            _client_count = count;
            auto message = std::make_shared<const std::vector<uint8_t>>(
                browser::clients_message(_last_sequence, narrow_cast<uint32_t>(count)));
            _transport.broadcast(std::move(message));
        },
        &transport_error);
    if(not started)
        throw U_EXCEPTION(transport_error);

    _image_thread = std::thread([this] { image_loop(); });
}

BrowserBase::~BrowserBase() {
    stop();
}

bool BrowserBase::start(std::string* error) {
    if(_transport.running())
        return true;
    if(error)
        *error = "Browser transport was not started.";
    return false;
}

void BrowserBase::stop() {
    {
        std::lock_guard guard(_image_mutex);
        _stop_images = true;
        _pending_images.clear();
    }
    _image_condition.notify_all();
    if(_image_thread.joinable())
        _image_thread.join();
    _transport.stop();
}

void BrowserBase::load_fonts(const BrowserBaseConfig& config) {
    auto load = [&](uint32_t style,
                    const file::Path& path,
                    Float2_t pixel_size,
                    ImVec2 glyph_offset = {})
    {
        ImFontConfig font_config;
        font_config.OversampleH = 5;
        font_config.OversampleV = 5;
        font_config.SizePixels = pixel_size;
        font_config.GlyphOffset = glyph_offset;
        ImFont* font = nullptr;
        if(not path.empty() && path.exists())
            font = _font_atlas.AddFontFromFileTTF(
                path.str().c_str(), pixel_size, &font_config, browser_glyph_ranges());
        if(not font)
            font = _font_atlas.AddFontDefault(&font_config);
        _fonts[style] = font;
    };
    load(Style::Regular, config.regular_font, 32_F);
    load(Style::Italic, config.italic_font, 32_F);
    load(Style::Bold, config.bold_font, 32_F);
    load(Style::Bold | Style::Italic, config.bold_italic_font, 32_F);
    load(Style::Monospace, config.monospace_font, 32_F * 0.85_F, {-1, 1.8});
    load(Style::Monospace | Style::Bold, config.monospace_bold_font, 32_F * 0.85_F, {-1, 1.8});
    load(Style::Symbols, config.symbols_font, 32_F, {0, 3});
    _font_atlas.Build();
}

ImFont* BrowserBase::font_for(uint32_t style) const {
    const auto face = browser::font_face_style(style);
    if(const auto found = _fonts.find(face); found != _fonts.end())
        return found->second;
    return _fonts.at(Style::Regular);
}

Bounds BrowserBase::text_bounds(const std::string& text, Drawable*, const Font& font) {
    const auto selected = font_for(font.style);
    const auto size = selected->CalcTextSizeA(selected->LegacySize * font.size, FLT_MAX, -1_F,
                                               text.data(), text.data() + text.size());
    return Bounds(0, 0, size.x, size.y);
}

Float2_t BrowserBase::line_spacing(const Font& font) {
    return font_for(font.style)->LegacySize * font.size;
}

void BrowserBase::set_title(std::string title) {
    _title = std::move(title);
}

void BrowserBase::set_window_size(Size2 size) {
    _window_bounds << size;
    _graph->set_size(size);
}

void BrowserBase::set_window_bounds(Bounds bounds) {
    _window_bounds = bounds;
    _graph->set_size(bounds.size());
}

void BrowserBase::set_frame_recording(bool value) {
    _frame_recording = false;
    if(value)
        FormatWarning("Screen recording is unavailable in browser-only GUI mode.");
}

LoopStatus BrowserBase::update_loop() {
    Base::process_main_queue();
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    return _transport.running() ? LoopStatus::UPDATED : LoopStatus::END;
}

void BrowserBase::paint(DrawStructure& graph) {
    const auto traversal = RenderTraversal::collect(graph, {
        .scale = graph.scale().div(gui::interface_scale()),
        .viewport = window_dimensions(),
        .cull = true
    });
    auto update = _scene_encoder.encode(traversal, window_dimensions());
    _last_sequence = update.sequence;
    for(const auto resource : update.removed_resources) {
        {
            std::lock_guard guard(_image_mutex);
            _pending_images.erase(resource);
            _active_image_revisions.erase(resource);
        }
        _transport.remove_resource(resource);
    }
    if(update.changed) {
        _transport.set_scene_snapshot(update.snapshot);
        _transport.broadcast(update.outbound);
    }
    queue_images(std::move(update.images));
}

void BrowserBase::queue_images(std::vector<browser::ImageJob> images) {
    if(images.empty())
        return;
    {
        std::lock_guard guard(_image_mutex);
        for(auto& image : images) {
            _active_image_revisions[image.resource_id] = image.revision;
            _pending_images[image.resource_id] = std::move(image);
        }
    }
    _image_condition.notify_one();
}

void BrowserBase::image_loop() {
    cmn::set_thread_name("browser-image-encoder");
    while(true) {
        browser::ImageJob job;
        {
            std::unique_lock guard(_image_mutex);
            _image_condition.wait(guard, [&] { return _stop_images || not _pending_images.empty(); });
            if(_stop_images)
                return;
            auto found = _pending_images.begin();
            job = std::move(found->second);
            _pending_images.erase(found);
        }

        auto encoded = browser::encode_image(*job.pixels, _image_quality.load());
        if(encoded.bytes.empty())
            continue;
        {
            std::lock_guard guard(_image_mutex);
            const auto active = _active_image_revisions.find(job.resource_id);
            if(active == _active_image_revisions.end() || active->second != job.revision)
                continue;

            auto message = std::make_shared<const std::vector<uint8_t>>(browser::image_message(
                _last_sequence,
                job.resource_id,
                job.revision,
                encoded.width,
                encoded.height,
                encoded.png,
                encoded.bytes));
            _transport.set_resource(job.resource_id, message);
            _transport.broadcast(std::move(message));
        }
    }
}

void BrowserBase::release_held_locked(browser::BrowserTransport::ClientId client) {
    const auto found = _held_input.find(client);
    if(found == _held_input.end())
        return;
    for(const auto& code : found->second.keys) {
        browser::InputPacket release;
        release.kind = browser::InputKind::Key;
        release.code = code;
        release.pressed = false;
        _discrete_input.emplace_back(client, std::move(release));
    }
    for(auto button : found->second.buttons) {
        browser::InputPacket release;
        release.kind = browser::InputKind::PointerButton;
        release.button = button;
        release.pressed = false;
        _discrete_input.emplace_back(client, std::move(release));
    }
    _held_input.erase(found);
}

void BrowserBase::receive(browser::BrowserTransport::ClientId client, const browser::ClientMessage& message) {
    if(message.type != browser::MessageType::Input || not message.input)
        return;
    std::lock_guard guard(_input_mutex);
    auto input = *message.input;
    if(input.kind == browser::InputKind::PointerMove) {
        _latest_pointer_move = std::pair{client, std::move(input)};
        return;
    }
    if(input.kind == browser::InputKind::Blur) {
        release_held_locked(client);
        if(_latest_pointer_move && _latest_pointer_move->first == client)
            _latest_pointer_move.reset();
        return;
    }
    if(input.kind == browser::InputKind::Key) {
        if(input.pressed)
            _held_input[client].keys.insert(input.code);
        else
            _held_input[client].keys.erase(input.code);
    } else if(input.kind == browser::InputKind::PointerButton) {
        if(input.pressed)
            _held_input[client].buttons.insert(input.button);
        else
            _held_input[client].buttons.erase(input.button);
    }
    _discrete_input.emplace_back(client, std::move(input));
}

void BrowserBase::disconnected(browser::BrowserTransport::ClientId client) {
    std::lock_guard guard(_input_mutex);
    release_held_locked(client);
    if(_latest_pointer_move && _latest_pointer_move->first == client)
        _latest_pointer_move.reset();
}

void BrowserBase::dispatch(DrawStructure& graph, const Event& event) {
    if(graph.event(event) && event.type != EventType::MMOVE)
        return;
    if(_event_callback) {
        try {
            _event_callback(graph, event);
        } catch(const std::invalid_argument&) {}
    }
}

void BrowserBase::process_input(DrawStructure& graph) {
    std::vector<browser::InputPacket> inputs;
    {
        std::lock_guard guard(_input_mutex);
        inputs.reserve(size_t(_latest_pointer_move.has_value()) + _discrete_input.size());
        if(_latest_pointer_move) {
            inputs.push_back(std::move(_latest_pointer_move->second));
            _latest_pointer_move.reset();
        }
        while(not _discrete_input.empty()) {
            inputs.push_back(std::move(_discrete_input.front().second));
            _discrete_input.pop_front();
        }
    }

    for(const auto& input : inputs) {
        switch(input.kind) {
            case browser::InputKind::PointerMove: {
                Event event(EventType::MMOVE);
                event.move = {input.x, input.y};
                dispatch(graph, event);
                break;
            }
            case browser::InputKind::PointerButton: {
                Event event(EventType::MBUTTON);
                event.mbutton = {input.button, input.pressed, false, input.x, input.y};
                dispatch(graph, event);
                break;
            }
            case browser::InputKind::Wheel: {
                Event event(EventType::SCROLL);
#if __linux__ || WIN32
                event.scroll = {input.dx / 15_F, input.dy / 15_F};
#else
                event.scroll = {input.dx, input.dy};
#endif
                dispatch(graph, event);
                break;
            }
            case browser::InputKind::Key: {
                const auto code = browser::browser_key_code(input.code);
                if(code == Keyboard::Unknown)
                    break;
                Event event(EventType::KEY);
                event.key = {
                    code,
                    input.pressed,
                    bool(input.modifiers & browser::Modifier::Shift),
                    bool(input.modifiers & browser::Modifier::Control),
                    bool(input.modifiers & browser::Modifier::Alt),
                    bool(input.modifiers & browser::Modifier::System),
                    input.repeat
                };
                dispatch(graph, event);
                break;
            }
            case browser::InputKind::Text: {
                Event event(EventType::TEXT_ENTERED);
                event.text = {
                    input.codepoint <= 0x7f ? static_cast<char>(input.codepoint) : char(0),
                    input.codepoint
                };
                dispatch(graph, event);
                break;
            }
            case browser::InputKind::Blur:
                break;
        }
    }
}
}
