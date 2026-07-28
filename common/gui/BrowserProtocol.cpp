#include "BrowserProtocol.h"

#include <gui/GuiTypes.h>
#include <gui/types/Drawable.h>

namespace cmn::gui::browser {
namespace {
class Writer {
    std::vector<uint8_t> _bytes;
public:
    explicit Writer(MessageType type, uint64_t sequence) {
        u32(protocol_magic);
        u16(protocol_version);
        u16(static_cast<uint16_t>(type));
        u64(sequence);
    }

    void u8(uint8_t value) { _bytes.push_back(value); }
    void u16(uint16_t value) {
        for(size_t i = 0; i < 2; ++i)
            _bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    void u32(uint32_t value) {
        for(size_t i = 0; i < 4; ++i)
            _bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    void u64(uint64_t value) {
        for(size_t i = 0; i < 8; ++i)
            _bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    void f32(float value) {
        u32(std::bit_cast<uint32_t>(value));
    }
    void bytes(std::span<const uint8_t> value) {
        _bytes.insert(_bytes.end(), value.begin(), value.end());
    }
    void string(std::string_view value) {
        u32(narrow_cast<uint32_t>(value.size()));
        bytes({reinterpret_cast<const uint8_t*>(value.data()), value.size()});
    }
    std::vector<uint8_t> take() { return std::move(_bytes); }
};

class PayloadWriter {
    std::vector<uint8_t> _bytes;
public:
    void u8(uint8_t value) { _bytes.push_back(value); }
    void u32(uint32_t value) {
        for(size_t i = 0; i < 4; ++i)
            _bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    void u64(uint64_t value) {
        for(size_t i = 0; i < 8; ++i)
            _bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    void f32(float value) { u32(std::bit_cast<uint32_t>(value)); }
    void color(const Color& color) {
        u8(color.r); u8(color.g); u8(color.b); u8(color.a);
    }
    void string(std::string_view value) {
        u32(narrow_cast<uint32_t>(value.size()));
        _bytes.insert(_bytes.end(), value.begin(), value.end());
    }
    std::vector<uint8_t> take() { return std::move(_bytes); }
};

class Reader {
    std::span<const uint8_t> _bytes;
    size_t _offset{0};
public:
    explicit Reader(std::span<const uint8_t> bytes) : _bytes(bytes) {}

    bool read_u8(uint8_t& value) {
        if(_offset + 1 > _bytes.size()) return false;
        value = _bytes[_offset++];
        return true;
    }
    bool read_u16(uint16_t& value) {
        if(_offset + 2 > _bytes.size()) return false;
        value = uint16_t(_bytes[_offset]) | (uint16_t(_bytes[_offset + 1]) << 8);
        _offset += 2;
        return true;
    }
    bool read_u32(uint32_t& value) {
        if(_offset + 4 > _bytes.size()) return false;
        value = 0;
        for(size_t i = 0; i < 4; ++i)
            value |= uint32_t(_bytes[_offset + i]) << (i * 8);
        _offset += 4;
        return true;
    }
    bool read_u64(uint64_t& value) {
        if(_offset + 8 > _bytes.size()) return false;
        value = 0;
        for(size_t i = 0; i < 8; ++i)
            value |= uint64_t(_bytes[_offset + i]) << (i * 8);
        _offset += 8;
        return true;
    }
    bool read_f32(float& value) {
        uint32_t bits;
        if(not read_u32(bits)) return false;
        value = std::bit_cast<float>(bits);
        return std::isfinite(value);
    }
    bool read_string(std::string& value, size_t max_size = 256) {
        uint32_t size;
        if(not read_u32(size) || size > max_size || _offset + size > _bytes.size()) return false;
        value.assign(reinterpret_cast<const char*>(_bytes.data() + _offset), size);
        _offset += size;
        return true;
    }
    bool empty() const { return _offset == _bytes.size(); }
};

void write_transform(PayloadWriter& writer, const Transform& transform) {
    const auto matrix = transform.getMatrix();
    writer.f32(static_cast<float>(matrix[0]));
    writer.f32(static_cast<float>(matrix[1]));
    writer.f32(static_cast<float>(matrix[4]));
    writer.f32(static_cast<float>(matrix[5]));
    writer.f32(static_cast<float>(matrix[12]));
    writer.f32(static_cast<float>(matrix[13]));
}

void write_clip(PayloadWriter& writer, const RenderCommand& command) {
    writer.u8(command.has_clip());
    if(command.has_clip()) {
        writer.f32(command._clip_rect.x);
        writer.f32(command._clip_rect.y);
        writer.f32(command._clip_rect.w - command._clip_rect.x);
        writer.f32(command._clip_rect.z - command._clip_rect.y);
    }
}

uint64_t image_revision(const Image& image) {
    return image.stamp().get();
}

struct SerializedPrimitive {
    std::vector<uint8_t> bytes;
    uint64_t image_revision{0};
    const Image* image{nullptr};
};

SerializedPrimitive serialize(const RenderCommand& command, uint32_t resource_id) {
    PayloadWriter writer;
    auto drawable = command.ptr;

    PrimitiveKind kind;
    if(command.type == RenderCommand::BACKGROUND || command.type == RenderCommand::BACKGROUND_LINE)
        kind = PrimitiveKind::Rect;
    else {
        switch(drawable->type()) {
            case Type::RECT: kind = PrimitiveKind::Rect; break;
            case Type::CIRCLE: kind = PrimitiveKind::Circle; break;
            case Type::TEXT: kind = PrimitiveKind::Text; break;
            case Type::POLYGON: kind = PrimitiveKind::Polygon; break;
            case Type::LINE:
            case Type::VERTICES: kind = PrimitiveKind::Vertices; break;
            case Type::IMAGE: kind = PrimitiveKind::Image; break;
            default: return {};
        }
    }

    writer.u8(static_cast<uint8_t>(kind));
    write_transform(writer, command.full_transform);
    write_clip(writer, command);

    SerializedPrimitive result;
    switch(kind) {
        case PrimitiveKind::Rect: {
            writer.f32(drawable->width());
            writer.f32(drawable->height());
            Color fill = Transparent;
            Color line = Transparent;
            CornerFlags corners = CornerFlags::Square();
            if(command.type == RenderCommand::BACKGROUND || command.type == RenderCommand::BACKGROUND_LINE) {
                const auto section = static_cast<SectionInterface*>(drawable);
                corners = section->corner_flags();
                if(command.type == RenderCommand::BACKGROUND)
                    fill = section->bg_fill_color();
                else
                    line = section->bg_line_color();
            } else {
                const auto rect = static_cast<Rect*>(drawable);
                fill = rect->fillclr();
                line = rect->lineclr();
                corners = rect->corners();
            }
            writer.color(fill);
            writer.color(line);
            writer.u8(corners.mask);
            writer.f32(corners.radius);
            break;
        }
        case PrimitiveKind::Circle: {
            const auto circle = static_cast<Circle*>(drawable);
            writer.f32(circle->radius());
            writer.color(circle->fill_clr());
            writer.color(circle->line_clr());
            break;
        }
        case PrimitiveKind::Text: {
            const auto text = static_cast<Text*>(drawable);
            writer.string(text->txt());
            writer.color(text->color());
            writer.f32(text->font().size);
            writer.u32(text->font().style);
            writer.u8(static_cast<uint8_t>(text->font().align));
            writer.f32(text->shadow());
            break;
        }
        case PrimitiveKind::Polygon: {
            const auto polygon = static_cast<Polygon*>(drawable);
            const auto points = polygon->relative();
            writer.u32(points ? narrow_cast<uint32_t>(points->size()) : 0);
            if(points) {
                for(const auto& point : *points) {
                    writer.f32(point.x);
                    writer.f32(point.y);
                }
            }
            writer.color(polygon->fill_clr());
            writer.color(polygon->border_clr());
            writer.u8(polygon->show_points());
            break;
        }
        case PrimitiveKind::Vertices: {
            auto vertices = static_cast<VertexArray*>(drawable);
            auto& points = vertices->points();
            writer.u8(static_cast<uint8_t>(vertices->primitive()));
            writer.f32(vertices->thickness());
            writer.u32(narrow_cast<uint32_t>(points.size()));
            for(const auto& point : points) {
                writer.f32(point.position().x);
                writer.f32(point.position().y);
                writer.color(point.clr());
            }
            break;
        }
        case PrimitiveKind::Image: {
            const auto image = static_cast<ExternalImage*>(drawable);
            const auto source = image->source();
            const auto revision = source ? image_revision(*source) : 0;
            writer.u32(resource_id);
            writer.u64(revision);
            writer.f32(drawable->width());
            writer.f32(drawable->height());
            writer.f32(source ? Float2_t(source->cols) : 0_F);
            writer.f32(source ? Float2_t(source->rows) : 0_F);
            writer.color(image->color() == Transparent ? White : image->color());
            result.image_revision = revision;
            result.image = source;
            break;
        }
    }

    result.bytes = writer.take();
    return result;
}

void write_scene_entries(Writer& writer,
                         const std::vector<std::pair<uint32_t, const std::vector<uint8_t>*>>& entries)
{
    writer.u32(narrow_cast<uint32_t>(entries.size()));
    for(const auto& [id, bytes] : entries) {
        writer.u32(id);
        writer.u32(narrow_cast<uint32_t>(bytes->size()));
        writer.bytes(*bytes);
    }
}

void write_order(Writer& writer, const std::vector<uint32_t>& order) {
    writer.u32(narrow_cast<uint32_t>(order.size()));
    for(auto id : order)
        writer.u32(id);
}
}

size_t SceneEncoder::KeyHash::operator()(const Key& key) const noexcept {
    const auto pointer_hash = std::hash<const void*>{}(key.drawable);
    return pointer_hash ^ (static_cast<size_t>(key.command) * 0x9e3779b97f4a7c15ULL);
}

FrameUpdate SceneEncoder::encode(const std::vector<RenderCommand>& commands, Size2 viewport) {
    std::unordered_map<uint32_t, std::vector<uint8_t>> current;
    std::vector<uint32_t> order;
    std::vector<ImageJob> images;
    current.reserve(commands.size());
    order.reserve(commands.size());

    for(const auto& command : commands) {
        if(command.type == RenderCommand::START_ROTATION || command.type == RenderCommand::END_ROTATION)
            continue;

        const Key key{command.ptr, command.type};
        auto [id_it, inserted] = _ids.try_emplace(key, _next_id);
        if(inserted)
            ++_next_id;
        const auto id = id_it->second;
        auto primitive = serialize(command, id);
        if(primitive.bytes.empty())
            continue;

        if(primitive.image && primitive.image_revision != 0) {
            const auto previous = _image_revisions.find(id);
            if(previous == _image_revisions.end() || previous->second != primitive.image_revision) {
                _image_revisions[id] = primitive.image_revision;
                images.push_back({id, primitive.image_revision, std::make_shared<Image>(*primitive.image)});
            }
        }
        current.emplace(id, std::move(primitive.bytes));
        order.push_back(id);
    }

    std::vector<std::pair<uint32_t, const std::vector<uint8_t>*>> upserts;
    std::vector<uint32_t> removals;
    for(const auto& [id, bytes] : current) {
        const auto previous = _previous.find(id);
        if(previous == _previous.end() || previous->second != bytes)
            upserts.emplace_back(id, &bytes);
    }
    for(const auto& [id, bytes] : _previous) {
        if(not current.contains(id))
            removals.push_back(id);
    }
    std::sort(upserts.begin(), upserts.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    std::sort(removals.begin(), removals.end());

    const bool order_changed = order != _previous_order;
    const bool viewport_changed = viewport != _previous_viewport;
    const bool first = _sequence == 0;
    const bool changed = first || viewport_changed || order_changed || not upserts.empty() || not removals.empty();
    if(changed)
        ++_sequence;

    FrameUpdate update;
    update.sequence = _sequence;
    update.changed = changed;
    update.first_snapshot = first;
    update.images = std::move(images);
    for(const auto id : removals) {
        if(_image_revisions.erase(id) > 0)
            update.removed_resources.push_back(id);
    }

    if(changed) {
        std::vector<std::pair<uint32_t, const std::vector<uint8_t>*>> all;
        all.reserve(current.size());
        for(const auto& [id, bytes] : current)
            all.emplace_back(id, &bytes);
        std::sort(all.begin(), all.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        Writer snapshot(MessageType::Snapshot, _sequence);
        snapshot.f32(viewport.width);
        snapshot.f32(viewport.height);
        write_scene_entries(snapshot, all);
        write_order(snapshot, order);
        update.snapshot = std::make_shared<const std::vector<uint8_t>>(snapshot.take());

        if(first) {
            update.outbound = update.snapshot;
        } else {
            Writer delta(MessageType::Delta, _sequence);
            delta.f32(viewport.width);
            delta.f32(viewport.height);
            write_scene_entries(delta, upserts);
            delta.u32(narrow_cast<uint32_t>(removals.size()));
            for(auto id : removals)
                delta.u32(id);
            delta.u8(order_changed);
            if(order_changed)
                write_order(delta, order);
            update.outbound = std::make_shared<const std::vector<uint8_t>>(delta.take());
        }
    }

    _previous = std::move(current);
    _previous_order = std::move(order);
    _previous_viewport = viewport;
    return update;
}

void SceneEncoder::reset() {
    _sequence = 0;
    _next_id = 1;
    _ids.clear();
    _previous.clear();
    _previous_order.clear();
    _previous_viewport = {};
    _image_revisions.clear();
}

std::vector<uint8_t> image_message(uint64_t sequence,
                                   uint32_t resource_id,
                                   uint64_t revision,
                                   uint32_t width,
                                   uint32_t height,
                                   bool png,
                                   std::span<const uint8_t> encoded)
{
    Writer writer(MessageType::Image, sequence);
    writer.u32(resource_id);
    writer.u64(revision);
    writer.u32(width);
    writer.u32(height);
    writer.u8(png ? 1 : 0);
    writer.u32(narrow_cast<uint32_t>(encoded.size()));
    writer.bytes(encoded);
    return writer.take();
}

std::vector<uint8_t> clients_message(uint64_t sequence, uint32_t clients) {
    Writer writer(MessageType::Clients, sequence);
    writer.u32(clients);
    return writer.take();
}

std::vector<uint8_t> error_message(uint64_t sequence, std::string_view message) {
    Writer writer(MessageType::Error, sequence);
    writer.string(message);
    return writer.take();
}

std::optional<ClientMessage> decode_client_message(std::span<const uint8_t> bytes, std::string* error) {
    auto fail = [&](std::string_view message) -> std::optional<ClientMessage> {
        if(error) *error = message;
        return std::nullopt;
    };

    Reader reader(bytes);
    uint32_t magic;
    uint16_t version;
    uint16_t raw_type;
    uint64_t sequence;
    if(not reader.read_u32(magic) || not reader.read_u16(version)
       || not reader.read_u16(raw_type) || not reader.read_u64(sequence))
        return fail("Truncated protocol header.");
    if(magic != protocol_magic)
        return fail("Invalid protocol magic.");
    if(version != protocol_version)
        return fail("Unsupported protocol version.");

    const auto type = static_cast<MessageType>(raw_type);
    ClientMessage message{.type = type, .sequence = sequence};
    if(type == MessageType::Resync || type == MessageType::Heartbeat) {
        if(not reader.empty()) return fail("Unexpected control-message payload.");
        return message;
    }
    if(type != MessageType::Input)
        return fail("Unsupported client message type.");

    InputPacket input;
    uint8_t raw_kind;
    if(not reader.read_u8(raw_kind) || not reader.read_u8(input.modifiers))
        return fail("Truncated input message.");
    input.kind = static_cast<InputKind>(raw_kind);
    switch(input.kind) {
        case InputKind::PointerMove:
            if(not reader.read_f32(input.x) || not reader.read_f32(input.y))
                return fail("Invalid pointer move.");
            break;
        case InputKind::PointerButton: {
            uint8_t pressed;
            if(not reader.read_u8(input.button) || input.button > 1 || not reader.read_u8(pressed)
               || not reader.read_f32(input.x) || not reader.read_f32(input.y))
                return fail("Invalid pointer button.");
            input.pressed = pressed != 0;
            break;
        }
        case InputKind::Wheel:
            if(not reader.read_f32(input.dx) || not reader.read_f32(input.dy))
                return fail("Invalid wheel input.");
            break;
        case InputKind::Key: {
            uint8_t pressed, repeat;
            if(not reader.read_u8(pressed) || not reader.read_u8(repeat) || not reader.read_string(input.code, 64))
                return fail("Invalid key input.");
            input.pressed = pressed != 0;
            input.repeat = repeat != 0;
            break;
        }
        case InputKind::Text:
            if(not reader.read_u32(input.codepoint) || input.codepoint > 0x10ffff
               || (input.codepoint >= 0xd800 && input.codepoint <= 0xdfff))
                return fail("Invalid Unicode codepoint.");
            break;
        case InputKind::Blur:
            break;
        default:
            return fail("Unknown input kind.");
    }
    if(not reader.empty())
        return fail("Unexpected trailing input data.");
    message.input = std::move(input);
    return message;
}

Keyboard::Codes browser_key_code(std::string_view code) {
    if(code.size() == 4 && code.substr(0, 3) == "Key" && code[3] >= 'A' && code[3] <= 'Z')
        return static_cast<Keyboard::Codes>(Keyboard::A + (code[3] - 'A'));
    if(code.size() == 6 && code.substr(0, 5) == "Digit" && code[5] >= '0' && code[5] <= '9')
        return static_cast<Keyboard::Codes>(Keyboard::Num0 + (code[5] - '0'));

    static const std::unordered_map<std::string_view, Keyboard::Codes> codes{
        {"Escape", Keyboard::Escape}, {"ControlLeft", Keyboard::LControl}, {"ControlRight", Keyboard::RControl},
        {"ShiftLeft", Keyboard::LShift}, {"ShiftRight", Keyboard::RShift}, {"AltLeft", Keyboard::LAlt},
        {"AltRight", Keyboard::RAlt}, {"MetaLeft", Keyboard::LSystem}, {"MetaRight", Keyboard::RSystem},
        {"BracketLeft", Keyboard::LBracket}, {"BracketRight", Keyboard::RBracket}, {"Semicolon", Keyboard::SemiColon},
        {"Comma", Keyboard::Comma}, {"Period", Keyboard::Period}, {"Quote", Keyboard::Quote},
        {"Slash", Keyboard::Slash}, {"Backslash", Keyboard::BackSlash}, {"Backquote", Keyboard::Tilde},
        {"Equal", Keyboard::Equal}, {"Minus", Keyboard::Dash}, {"Space", Keyboard::Space},
        {"Enter", Keyboard::Return}, {"Backspace", Keyboard::BackSpace}, {"Tab", Keyboard::Tab},
        {"PageUp", Keyboard::PageUp}, {"PageDown", Keyboard::PageDown}, {"End", Keyboard::End},
        {"Home", Keyboard::Home}, {"Insert", Keyboard::Insert}, {"Delete", Keyboard::Delete},
        {"ArrowLeft", Keyboard::Left}, {"ArrowRight", Keyboard::Right}, {"ArrowUp", Keyboard::Up},
        {"ArrowDown", Keyboard::Down}, {"Pause", Keyboard::Pause}
    };
    if(const auto found = codes.find(code); found != codes.end())
        return found->second;
    if(code.size() >= 2 && code[0] == 'F') {
        try {
            const int number = std::stoi(std::string(code.substr(1)));
            if(number >= 1 && number <= 15)
                return static_cast<Keyboard::Codes>(Keyboard::F1 + number - 1);
        } catch(...) {}
    }
    return Keyboard::Unknown;
}
}
