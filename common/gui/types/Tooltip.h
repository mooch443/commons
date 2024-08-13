#pragma once

#include <gui/types/Entangled.h>
#include <gui/types/StaticText.h>

namespace cmn::gui {
    class Tooltip : public Entangled {
        GETTER_PTR(std::weak_ptr<Drawable>, other);
        GETTER_NCONST(StaticText, text);
        float _max_width;
        
    public:
        Tooltip(std::nullptr_t, float max_width = -1);
        Tooltip(std::weak_ptr<Drawable> other, float max_width = -1);
        void set_text(const std::string& text);
        void set_other(std::weak_ptr<Drawable> other);
        
    protected:
        virtual void update() override;
    };
}
