#pragma once

#include <gui/types/Tooltip.h>
#include <misc/GlobalSettings.h>

namespace cmn::gui {

class SettingsTooltip : public Tooltip {
    const sprite::Map* map{nullptr};
    const GlobalSettings::docs_map_t* docs{nullptr};
    std::string _param;
    
public:
    SettingsTooltip(std::weak_ptr<Drawable> ptr = {},
                    const sprite::Map* map = nullptr,
                    const GlobalSettings::docs_map_t* docs = nullptr);
    void set_parameter(const std::string& name);
    void update() override;
};

}
