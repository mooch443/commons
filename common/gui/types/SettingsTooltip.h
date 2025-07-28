#pragma once

#include <gui/types/Tooltip.h>
#include <misc/GlobalSettings.h>

namespace cmn::gui {

class SettingsTooltip : public Tooltip {
    std::string _param;
    
public:
    SettingsTooltip(std::weak_ptr<Drawable> ptr = {});
    void set_parameter(const std::string& name);
    void update() override;
};

}
