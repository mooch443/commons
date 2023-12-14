#pragma once

#include <gui/ControlsAttributes.h>

namespace gui {
    ATTRIBUTE_ALIAS(OnHover_t, std::function<void(size_t)>)
    NUMBER_ALIAS(Foldable_t, bool)
    NUMBER_ALIAS(Folded_t, bool)
    NUMBER_ALIAS(Alternating_t, bool)
    ATTRIBUTE_ALIAS(ItemFont_t, Font)
    ATTRIBUTE_ALIAS(ItemPadding_t, Vec2)
    ATTRIBUTE_ALIAS(ListDims_t, Size2)
    ATTRIBUTE_ALIAS(LabelDims_t, Size2)
    ATTRIBUTE_ALIAS(LabelFont_t, Font)
    ATTRIBUTE_ALIAS(DetailColor_t, Color)
    ATTRIBUTE_ALIAS(ListFillClr_t, Color)
    ATTRIBUTE_ALIAS(ListLineClr_t, Color)
    ATTRIBUTE_ALIAS(LabelColor_t, Color)
    ATTRIBUTE_ALIAS(LabelBorderColor_t, Color)
    ATTRIBUTE_ALIAS(ItemColor_t, Color)
    ATTRIBUTE_ALIAS(ItemBorderColor_t, Color)
}
