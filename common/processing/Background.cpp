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
        _name = "Background"+Meta::toStr((uint64_t)this);
        _callback = _name.c_str();
        
        GlobalSettings::map().register_callback(_callback, [this](sprite::Map::Signal signal, sprite::Map&map, auto& name, auto&v){
            if(signal == sprite::Map::Signal::EXIT) {
                map.unregister_callback(_callback);
                _callback = nullptr;
                return;
            }
            
            if(name == "track_absolute_difference") {
                cmn::track_absolute_difference = v.template value<bool>();
                this->update_callback();
            } else if(name == "track_background_subtraction") {
                cmn::track_background_subtraction = v.template value<bool>();
                this->update_callback();
            }
        });
        
        cmn::track_absolute_difference = SETTING(track_absolute_difference).value<bool>();
        cmn::track_background_subtraction = SETTING(track_background_subtraction).value<bool>();
        update_callback();
    }
    
    Background::~Background() {
        if(_callback)
            GlobalSettings::map().unregister_callback(_callback);
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
