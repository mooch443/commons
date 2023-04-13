#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> track_absolute_difference = true,
                             use_differences = true;
    bool Background::track_absolute_difference() {
        return cmn::track_absolute_difference;
    }
    bool Background::use_differences() {
        return cmn::use_differences;
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
            } else if(name == "use_differences") {
                cmn::use_differences = v.template value<bool>();
                this->update_callback();
            }
        });
        
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
