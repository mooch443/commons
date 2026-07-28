#include "RenderTraversal.h"

#include <gui/DrawStructure.h>
#include <gui/types/Entangled.h>

namespace cmn::gui {
namespace {
class Collector {
    const RenderTraversalOptions& _options;
    std::vector<RenderCommand> _normal;
    std::vector<RenderCommand> _above;

    void append(RenderCommand::Type type,
                Drawable* drawable,
                const Transform& transform,
                const Transform& full_transform,
                const Bounds& bounds,
                const ImVec4& clip)
    {
        // Rotation sentinels delimit ranges in the native renderer and must
        // retain their original traversal positions. Primitive renderers use
        // full_transform directly and ignore these sentinels.
        const bool rotation_sentinel =
            type == RenderCommand::START_ROTATION || type == RenderCommand::END_ROTATION;
        auto& target = rotation_sentinel || drawable->z_index() == 0 ? _normal : _above;
        target.emplace_back(type,
                            _normal.size() + _above.size(),
                            drawable,
                            transform,
                            full_transform,
                            bounds,
                            clip);
    }

    static bool scrolls(Entangled* entangled) {
        return entangled->scroll_enabled() && entangled->size().max() > 0;
    }

    void visit(Drawable* drawable, ImVec4 clip = {}) {
        if(not drawable)
            return;

        drawable->set_was_visible(false);

        Transform transform;
        transform.scale(_options.scale);
        transform.combine(drawable->global_transform_no_rotation());

        Transform full_transform;
        full_transform.scale(_options.scale);
        full_transform.combine(drawable->global_transform());

        const auto bounds = transform.transformRect(Bounds(0, 0, drawable->width(), drawable->height()));
        if(_options.cull
           && _options.viewport.width > 0
           && _options.viewport.height > 0
           && drawable->type() != Type::ENTANGLED
           && drawable->type() != Type::SECTION
           && not Bounds(0, 0, _options.viewport.width, _options.viewport.height).overlaps(bounds))
        {
            return;
        }

        drawable->set_was_visible(true);

        switch(drawable->type()) {
            case Type::SECTION: {
                auto section = static_cast<SectionInterface*>(drawable);
                if(section->rotation() != 0)
                    append(RenderCommand::START_ROTATION, drawable, transform, full_transform, bounds, clip);

                if(section->bg().fill)
                    append(RenderCommand::BACKGROUND, drawable, transform, full_transform, bounds, clip);

                apply_to_objects(section->children(), [&](Drawable* child) {
                    visit(child, clip);
                });

                if(section->bg().line)
                    append(RenderCommand::BACKGROUND_LINE, drawable, transform, full_transform, bounds, clip);

                if(section->rotation() != 0)
                    append(RenderCommand::END_ROTATION, drawable, transform, full_transform, bounds, clip);
                break;
            }
            case Type::ENTANGLED: {
                auto entangled = static_cast<Entangled*>(drawable);
                if(entangled->rotation() != 0)
                    append(RenderCommand::START_ROTATION, drawable, transform, full_transform, bounds, clip);

                const auto parent_clip = clip;
                if(entangled->bg().fill)
                    append(RenderCommand::BACKGROUND, drawable, transform, full_transform, bounds, clip);

                if(scrolls(entangled)) {
                    clip = bounds;
                    for(auto child : entangled->children()) {
                        apply_to_object(child, [&](Drawable* object) {
                            if(entangled->scroll_enabled()) {
                                const auto local = object->local_bounds();
                                if(local.y < -local.height || local.y > entangled->height()
                                   || local.x < -local.width || local.x > entangled->width())
                                {
                                    return;
                                }
                            }
                            visit(object, clip);
                        });
                    }
                } else {
                    apply_to_objects(entangled->children(), [&](Drawable* child) {
                        visit(child, clip);
                    });
                }

                if(entangled->bg().line)
                    append(RenderCommand::BACKGROUND_LINE, drawable, transform, full_transform, bounds, parent_clip);

                if(entangled->rotation() != 0)
                    append(RenderCommand::END_ROTATION, drawable, transform, full_transform, bounds, clip);
                break;
            }
            default:
                append(RenderCommand::DEFAULT, drawable, transform, full_transform, bounds, clip);
                break;
        }
    }

public:
    explicit Collector(const RenderTraversalOptions& options)
        : _options(options)
    {}

    std::vector<RenderCommand> collect(DrawStructure& graph) {
        auto objects = graph.collect();
        apply_to_objects(objects, [&](Drawable* drawable) {
            visit(drawable);
        });

        std::sort(_above.begin(), _above.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ptr->z_index() < rhs.ptr->z_index()
                || (lhs.ptr->z_index() == rhs.ptr->z_index() && lhs.index < rhs.index);
        });
        _normal.insert(_normal.end(), _above.begin(), _above.end());
        return _normal;
    }
};
}

std::vector<RenderCommand> RenderTraversal::collect(DrawStructure& graph, const RenderTraversalOptions& options) {
    return Collector(options).collect(graph);
}
}
