#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> track_absolute_difference = true,
                             track_background_subtraction = true;
    bool Background::track_absolute_difference() {
        return cmn::track_absolute_difference;
    }
    bool Background::track_background_subtraction() {
        return cmn::track_background_subtraction;
    }

    Background::Background(Image::Ptr&& image, LuminanceGrid *grid)
        : _image(std::move(image)), _grid(grid), _bounds(_image->bounds())
    {
        auto _callback = GlobalSettings::map().register_callbacks({"track_absolute_difference","track_background_subtraction"}, [this](std::string_view name) {
            if(name == "track_absolute_difference") {
                cmn::track_absolute_difference = SETTING(track_absolute_difference).value<bool>();
            } else if(name == "track_background_subtraction") {
                cmn::track_background_subtraction = SETTING(track_background_subtraction).value<bool>();
            }
            update_callback();
        });
    }
    
    Background::~Background() {
        if(_callback)
            GlobalSettings::map().unregister_callbacks(std::move(_callback));
    }

    void Background::update_callback() {
#ifndef NDEBUG
        if(!SETTING(quiet))
            print("Updating static background difference method.");
#endif
    }
    
    int Background::color(coord_t x, coord_t y) const {
        return _image->data()[x + y * _image->cols];
    }
    
    
    const Image& Background::image() const {
        return *_image;
    }
    
    const Bounds& Background::bounds() const {
        return _bounds;
    }
}
