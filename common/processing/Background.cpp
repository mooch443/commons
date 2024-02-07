#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> track_absolute_difference = true,
        track_background_subtraction = true;
    static std::atomic<meta_encoding_t::data::values> meta_encoding = cmn::meta_encoding_t::gray.value();

    static auto check_callbacks = []() {
        static std::once_flag _check_callbacks;
        std::call_once(_check_callbacks, []() {
            auto _callback = GlobalSettings::map().register_callbacks({
                "track_absolute_difference",
                "track_background_subtraction",
                "meta_encoding"

            }, [](std::string_view name) {
                if (name == "track_absolute_difference") {
                    cmn::track_absolute_difference = SETTING(track_absolute_difference).value<bool>();
                }
                else if (name == "track_background_subtraction") {
                    cmn::track_background_subtraction = SETTING(track_background_subtraction).value<bool>();
                }
                else if (name == "meta_encoding") {
                    cmn::meta_encoding = SETTING(meta_encoding).value<meta_encoding_t::Class>().value();

                    //print("updated meta_encoding to ", meta_encoding_t::Class(cmn::meta_encoding.load()));
                }
                //Background::update_callback();
            });
		});
    };

    bool Background::track_absolute_difference() {
        check_callbacks();
        return cmn::track_absolute_difference;
    }
    bool Background::track_background_subtraction() {
        check_callbacks();
        return cmn::track_background_subtraction;
    }
    meta_encoding_t::data::values Background::meta_encoding() {
        check_callbacks();
        return cmn::meta_encoding;
    }
    ImageMode Background::image_mode() {
        check_callbacks();
        return cmn::meta_encoding == meta_encoding_t::gray
            ? ImageMode::GRAY
            : ImageMode::R3G3B2;
    }

    Background::Background(Image::Ptr&& image, LuminanceGrid *grid)
        : _image(std::move(image)), _grid(grid), _bounds(_image->bounds())
    {
        
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
