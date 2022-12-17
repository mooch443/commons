#pragma once

#include <misc/Image.h>
#include <processing/LuminanceGrid.h>

namespace cmn {
    enum class DifferenceMethod {
        absolute,
        sign
    };

    template<DifferenceMethod method>
    struct DifferenceImpl {
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::absolute)
        inline int operator()(int source, int value) const {
            return abs(source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::sign)
        inline int operator()(int source, int value) const {
            return max(0, source - value);
        }
    };

    class Background {
    public:
        static bool enable_absolute_difference();
        
    protected:
        Image::Ptr _image;
        LuminanceGrid* _grid;
        Bounds _bounds;
        //int (*_diff)(int, int);
        std::string _name;
        const char* _callback;
        
    public:
        Background(const Image::Ptr& image, LuminanceGrid* grid);
        ~Background();
        
        template<DifferenceMethod method>
        int diff(coord_t x, coord_t y, int value) const {
            return DifferenceImpl<method>{}(_image->data()[ptr_safe_t(x) + ptr_safe_t(y) * ptr_safe_t(_image->cols)], value);
        }
        
        template<DifferenceMethod method>
        bool is_different(coord_t x, coord_t y, int value, int threshold) const {
            return is_value_different(x, y, diff<method>(x, y, value), threshold);
        }
        
        bool is_value_different(coord_t x, coord_t y, int value, int threshold) const {
            assert(x < _image->cols && y < _image->rows);
            return value >= (_grid ? _grid->relative_threshold(x, y) : 1) * threshold;
        }
        
        coord_t count_above_threshold(coord_t x0, coord_t x1, coord_t y, const uchar* values, int threshold) const;
        int color(coord_t x, coord_t y) const;
        const Image& image() const;
        const Bounds& bounds() const;
        const LuminanceGrid* grid() const {
            return _grid;
        }
        
    private:
        void update_callback();
    };
}
