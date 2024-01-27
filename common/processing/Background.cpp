#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> track_absolute_difference = true,
                             track_background_subtraction = true;
    static std::atomic<meta_encoding_t::data::values> meta_encoding = cmn::meta_encoding_t::gray.value();

    bool Background::track_absolute_difference() {
        return cmn::track_absolute_difference;
    }
    bool Background::track_background_subtraction() {
        return cmn::track_background_subtraction;
    }
    ImageMode Background::meta_encoding() {
        return cmn::meta_encoding == meta_encoding_t::gray
            ? ImageMode::GRAY
            : ImageMode::R3G3B2;
    }

    Background::Background(Image::Ptr&& image, LuminanceGrid *grid)
        : _image(std::move(image)), _grid(grid), _bounds(_image->bounds())
    {
        auto _callback = GlobalSettings::map().register_callbacks({
            "track_absolute_difference", 
            "track_background_subtraction",
            "meta_encoding"
            
        }, [this](std::string_view name) {
            if(name == "track_absolute_difference") {
                cmn::track_absolute_difference = SETTING(track_absolute_difference).value<bool>();
            } else if(name == "track_background_subtraction") {
                cmn::track_background_subtraction = SETTING(track_background_subtraction).value<bool>();
            } else if(name == "meta_encoding") {
                cmn::meta_encoding = SETTING(meta_encoding).value<meta_encoding_t::Class>().value();
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
        if(GlobalSettings::is_runtime_quiet())
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
