#pragma once

#include <gui/types/Tooltip.h>
#include <misc/GlobalSettings.h>
#include <misc/TooltipData.h>

namespace cmn::gui {

class SettingsTooltip : public Tooltip {
    TooltipData _param;
    
public:
    SettingsTooltip(std::weak_ptr<Drawable> ptr = {});
    void set_parameter(TooltipData name);
    void update() override;
};

}
