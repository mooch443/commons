#pragma once

#include <commons.pc.h>
#include <misc/Image.h>
#include <processing/LuminanceGrid.h>

namespace cmn {
    enum class DifferenceMethod {
        absolute,
        sign,
        none
    };

    ENUM_CLASS(meta_encoding_t, gray, r3g3b2);

    template<DifferenceMethod method, ImageMode colors>
    struct DifferenceImpl {
        static int convert_to_grayscale(int r3g3b2) {
            if constexpr(colors == ImageMode::R3G3B2) {
                auto c = r3g3b2_to_vec<3>((uint8_t)r3g3b2);
                return saturate(((int64_t)c[0] + (int64_t)c[1] + (int64_t)c[2]) / 3, 0, 255);
            } else
                return r3g3b2;
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::absolute)
        inline int operator()(int source, int value) const {
            if constexpr(colors == ImageMode::R3G3B2) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return abs(source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::sign)
        inline int operator()(int source, int value) const {
            if constexpr(colors == ImageMode::R3G3B2) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return max(0, source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::none)
        inline int operator()(int, int value) const {
            if constexpr(colors == ImageMode::R3G3B2) {
                value = convert_to_grayscale(value);
            }
            return value;
        }
    };

    class Background {
    public:
        static bool track_absolute_difference();
        static bool track_background_subtraction();
        static meta_encoding_t::data::values meta_encoding();
        static ImageMode image_mode();
        
    protected:
        Image::Ptr _image;
        LuminanceGrid* _grid;
        Bounds _bounds;
        //int (*_diff)(int, int);
        std::string _name;
        CallbackCollection _callback;
        
    public:
        Background(Image::Ptr&& image, LuminanceGrid* grid);
        ~Background();
        
        template<DifferenceMethod method, ImageMode colors>
        int diff(coord_t x, coord_t y, int value) const {
            return DifferenceImpl<method, colors>{}(_image->data()[ptr_safe_t(x) + ptr_safe_t(y) * ptr_safe_t(_image->cols)], value);
        }
        
        template<DifferenceMethod method, ImageMode colors>
        bool is_different(coord_t x, coord_t y, int value, int threshold) const {
            return is_value_different(x, y, diff<method, colors>(x, y, value), threshold);
        }
        
        bool is_value_different(coord_t x, coord_t y, int value, int threshold) const {
            assert(x < _image->cols && y < _image->rows);
            return value >= (_grid ? _grid->relative_threshold(x, y) : 1) * threshold;
        }
        
        template<DifferenceMethod method>
        coord_t count_above_threshold(coord_t x0, coord_t x1, coord_t y, const uchar* values, int threshold) const
        {
            auto ptr_grid = _grid
                ? (_grid->thresholds().data()
                    + ptr_safe_t(x0) + ptr_safe_t(y) * (ptr_safe_t)_grid->bounds().width)
                : NULL;
            auto ptr_image = _image->data() + ptr_safe_t(x0) + ptr_safe_t(y) * ptr_safe_t(_image->cols);
            auto end = values + ptr_safe_t(x1) - ptr_safe_t(x0) + 1;
            ptr_safe_t count = 0;
            
            if constexpr (method == DifferenceMethod::sign) {
                if(ptr_grid) {
                    for (; values != end; ++ptr_grid, ++ptr_image, ++values)
                        count += int32_t(*ptr_image) - int32_t(*values) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; values != end; ++ptr_image, ++values)
                        count += int32_t(*ptr_image) - int32_t(*values) >= int32_t(threshold);
                }
                
            } else if constexpr(method == DifferenceMethod::absolute) {
                if(ptr_grid) {
                    for (; values != end; ++ptr_grid, ++ptr_image, ++values)
                        count += std::abs(int32_t(*ptr_image) - int32_t(*values)) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; values != end; ++ptr_image, ++values)
                        count += std::abs(int32_t(*ptr_image) - int32_t(*values)) >= int32_t(threshold);
                }
            } else {
                if(ptr_grid) {
                    for (; values != end; ++ptr_grid, ++values)
                        count += int32_t(*values) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; values != end; ++values)
                        count += int32_t(*values) >= int32_t(threshold);
                }
            }
            
            return count;
        }
        
        int color(coord_t x, coord_t y) const;
        const Image& image() const;
        const Bounds& bounds() const;
        const LuminanceGrid* grid() const {
            return _grid;
        }
        
    private:
        void update_callback();
    };

    auto call_image_mode_function(auto&& fn) {
        auto determine_colors = [&]<DifferenceMethod method>() {
            auto encoding = Background::image_mode();
            if(encoding == ImageMode::R3G3B2)
                return fn.template operator()<method, ImageMode::R3G3B2>();
            else if(encoding == ImageMode::GRAY)
                return fn.template operator()<method, ImageMode::GRAY>();
            else if(encoding == ImageMode::RGB)
                return fn.template operator()<method, ImageMode::RGB>();
            else if(encoding == ImageMode::RGBA)
                return fn.template operator()<method, ImageMode::RGBA>();
            else
                throw InvalidArgumentException("Invalid color mode for image.");
        };
        
        if(not Background::track_background_subtraction()) {
            return determine_colors.template operator()<DifferenceMethod::none>();
        } else if(Background::track_absolute_difference()) {
            return determine_colors.template operator()<DifferenceMethod::absolute>();
        } else {
            return determine_colors.template operator()<DifferenceMethod::sign>();
        }
    }
}
