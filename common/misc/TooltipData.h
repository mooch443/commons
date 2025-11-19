#pragma once
#include <commons.pc.h>

namespace cmn::gui {

struct TooltipData {
    struct Both {
        std::string name;
        std::string docs;
        bool operator==(const Both& other) const = default;
        auto operator<=>(const Both& other) const = default;
        bool operator!=(const Both& other) const { return !(*this == other); }
    };
    std::variant<std::string, Both> data;
    bool operator==(const TooltipData& other) const = default;
    auto operator<=>(const TooltipData& other) const = default;
    bool operator!=(const TooltipData& other) const { return !(*this == other); }
    
    bool is_name() const {
        return std::holds_alternative<std::string>(data);
    }
    std::string_view title() const {
        if(is_name()) {
            return std::get<std::string>(data);
        } else {
            const Both &both = std::get<Both>(data);
            return both.name;
        }
    }
    
    std::string text() const;
    
    std::string toStr() const;
    static std::string class_name() { return "TooltipData"; }
};

}

