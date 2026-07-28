#pragma once

#include <commons.pc.h>
#include <gui/Event.h>
#include <gui/RenderTraversal.h>
#include <misc/Image.h>

namespace cmn::gui::browser {
inline constexpr uint32_t protocol_magic = 0x58525442; // "BTRX" in little endian.
inline constexpr uint16_t protocol_version = 1;

enum class MessageType : uint16_t {
    Snapshot = 1,
    Delta = 2,
    Image = 3,
    Clients = 4,
    Input = 5,
    Resync = 6,
    Heartbeat = 7,
    Error = 8
};

enum class PrimitiveKind : uint8_t {
    Rect = 1,
    Circle = 2,
    Text = 3,
    Polygon = 4,
    Vertices = 5,
    Image = 6
};

enum class InputKind : uint8_t {
    PointerMove = 1,
    PointerButton = 2,
    Wheel = 3,
    Key = 4,
    Text = 5,
    Blur = 6
};

enum Modifier : uint8_t {
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    System = 1 << 3
};

struct ImageJob {
    uint32_t resource_id{0};
    uint64_t revision{0};
    std::shared_ptr<const Image> pixels;
};

struct FrameUpdate {
    uint64_t sequence{0};
    bool changed{false};
    bool first_snapshot{false};
    std::shared_ptr<const std::vector<uint8_t>> outbound;
    std::shared_ptr<const std::vector<uint8_t>> snapshot;
    std::vector<ImageJob> images;
    std::vector<uint32_t> removed_resources;
};

struct InputPacket {
    InputKind kind{InputKind::PointerMove};
    uint8_t modifiers{0};
    float x{0};
    float y{0};
    float dx{0};
    float dy{0};
    uint8_t button{0};
    bool pressed{false};
    bool repeat{false};
    std::string code;
    uint32_t codepoint{0};
};

struct ClientMessage {
    MessageType type{MessageType::Error};
    uint64_t sequence{0};
    std::optional<InputPacket> input;
};

class SceneEncoder {
    struct Key {
        const Drawable* drawable{nullptr};
        RenderCommand::Type command{RenderCommand::DEFAULT};
        bool operator==(const Key&) const noexcept = default;
    };
    struct KeyHash {
        size_t operator()(const Key&) const noexcept;
    };

    uint64_t _sequence{0};
    uint32_t _next_id{1};
    std::unordered_map<Key, uint32_t, KeyHash> _ids;
    std::unordered_map<uint32_t, std::vector<uint8_t>> _previous;
    std::vector<uint32_t> _previous_order;
    Size2 _previous_viewport;
    std::unordered_map<uint32_t, uint64_t> _image_revisions;

public:
    FrameUpdate encode(const std::vector<RenderCommand>&, Size2 viewport);
    void reset();
};

std::vector<uint8_t> image_message(uint64_t sequence,
                                   uint32_t resource_id,
                                   uint64_t revision,
                                   uint32_t width,
                                   uint32_t height,
                                   bool png,
                                   std::span<const uint8_t> encoded);
std::vector<uint8_t> clients_message(uint64_t sequence, uint32_t clients);
std::vector<uint8_t> error_message(uint64_t sequence, std::string_view message);
std::optional<ClientMessage> decode_client_message(std::span<const uint8_t>, std::string* error = nullptr);
Keyboard::Codes browser_key_code(std::string_view code);
}
