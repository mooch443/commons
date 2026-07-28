#pragma once

#include <commons.pc.h>
#include <gui/Transform.h>

namespace cmn::gui {
class DrawStructure;
class Drawable;

struct RenderCommand {
    enum Type : uint8_t {
        DEFAULT = 0,
        BACKGROUND,
        BACKGROUND_LINE,
        END_ROTATION,
        START_ROTATION
    };

    Type type{DEFAULT};
    size_t index{0};
    Drawable* ptr{nullptr};
    Transform transform;
    Transform full_transform;
    Bounds bounds;
    ImVec4 _clip_rect;

    RenderCommand() = default;
    RenderCommand(Type type,
                  size_t index,
                  Drawable* ptr,
                  const Transform& transform,
                  const Transform& full_transform,
                  const Bounds& bounds,
                  const ImVec4& clip)
        : type(type),
          index(index),
          ptr(ptr),
          transform(transform),
          full_transform(full_transform),
          bounds(bounds),
          _clip_rect(clip)
    {}

    RenderCommand(Type type,
                  size_t index,
                  Drawable* ptr,
                  const Transform& transform,
                  const Bounds& bounds,
                  const ImVec4& clip)
        : RenderCommand(type, index, ptr, transform, transform, bounds, clip)
    {}

    bool has_clip() const {
        // Bounds converts to ImVec4 as left, top, bottom, right to match the
        // long-standing native renderer convention.
        return _clip_rect.w > _clip_rect.x && _clip_rect.z > _clip_rect.y;
    }
};

struct RenderTraversalOptions {
    Vec2 scale{1_F, 1_F};
    Size2 viewport;
    bool cull{true};
};

class RenderTraversal {
public:
    static std::vector<RenderCommand> collect(DrawStructure&, const RenderTraversalOptions&);
};
}
