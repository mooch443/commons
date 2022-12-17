#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> enable_absolute_difference = true;
    bool Background::enable_absolute_difference() {
        return cmn::enable_absolute_difference;
    }

    Background::Background(const Image::Ptr& image, LuminanceGrid *grid)
        : _image(image), _grid(grid), _bounds(image->bounds())
    {
        _name = "Background"+Meta::toStr((uint64_t)this);
        _callback = _name.c_str();
        
        GlobalSettings::map().register_callback(_callback, [this](sprite::Map::Signal signal, sprite::Map&map, auto& name, auto&v){
            if(signal == sprite::Map::Signal::EXIT) {
                map.unregister_callback(_callback);
                _callback = nullptr;
                return;
            }
            
            if(name == "enable_absolute_difference") {
                cmn::enable_absolute_difference = v.template value<bool>();
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
    
    
    
    coord_t Background::count_above_threshold(coord_t x0, coord_t x1, coord_t y, const uchar* values, int threshold) const
    {
        auto ptr_grid = _grid 
            ? (_grid->thresholds().data() 
                + ptr_safe_t(x0) + ptr_safe_t(y) * (ptr_safe_t)_grid->bounds().width) 
            : NULL;
        auto ptr_image = _image->data() + ptr_safe_t(x0) + ptr_safe_t(y) * ptr_safe_t(_image->cols);
        auto end = values + ptr_safe_t(x1) - ptr_safe_t(x0) + 1;
        ptr_safe_t count = 0;
        
        if(!enable_absolute_difference()) {
            if(ptr_grid) {
                for (; values != end; ++ptr_grid, ++ptr_image, ++values)
                    count += int32_t(*ptr_image) - int32_t(*values) >= int32_t(*ptr_grid) * threshold;
            } else {
                for (; values != end; ++ptr_image, ++values)
                    count += int32_t(*ptr_image) - int32_t(*values) >= int32_t(threshold);
            }
            
        } else {
            if(ptr_grid) {
                for (; values != end; ++ptr_grid, ++ptr_image, ++values)
                    count += cmn::abs(int32_t(*ptr_image) - int32_t(*values)) >= int32_t(*ptr_grid) * threshold;
            } else {
                for (; values != end; ++ptr_image, ++values)
                    count += cmn::abs(int32_t(*ptr_image) - int32_t(*values)) >= int32_t(threshold);
            }
        }
        
        return count;
    }
    
    const Image& Background::image() const {
        return *_image;
    }
    
    const Bounds& Background::bounds() const {
        return _bounds;
    }
}
