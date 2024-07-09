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

    ENUM_CLASS(meta_encoding_t, gray, r3g3b2, rgb8);

    template <typename From, typename To>
    concept explicitly_convertible_to = requires(From from) {
        { static_cast<To>(from) };
    };

#define IMAGE_INFO_METHODS(NAME) \
    uint8_t channels; \
    meta_encoding_t::Class encoding; \
    \
    constexpr bool is_r3g3b2() const { \
        return meta_encoding_t::r3g3b2 == encoding; \
    } \
    \
    std::string toStr() const { \
        return std::string( #NAME ) + "<channels:"+Meta::toStr(channels) + " encoding:" + Meta::toStr(encoding)+">"; \
    } \
    static std::string class_name() { return #NAME ; } \
    \
    template<typename Info> \
        requires explicitly_convertible_to<Info, ImageInfo> \
    constexpr bool operator==(const Info& other) const { \
        return channels == other.channels && encoding == other.encoding; \
    }\
    template<typename Info> \
        requires explicitly_convertible_to<Info, ImageInfo> \
    constexpr bool operator!=(const Info& other) const { \
        return channels != other.channels || encoding != other.encoding; \
    } \
    \
    template<typename Info> \
    constexpr NAME & operator=(const Info& info) { \
        auto& [c, e] = info; \
        channels = c; \
        encoding = e; \
        return *this; \
    }

    struct ImageInfo {
        IMAGE_INFO_METHODS(ImageInfo)
    };


    struct InputInfo {
        IMAGE_INFO_METHODS(InputInfo)

        explicit constexpr operator ImageInfo() const {
            return ImageInfo{channels, encoding};
        }
    };

    struct OutputInfo {
        IMAGE_INFO_METHODS(OutputInfo)
        
        explicit constexpr operator ImageInfo() const {
            return ImageInfo{channels, encoding};
        }
    };

    static constexpr OutputInfo DIFFERENCE_OUTPUT_FORMAT {
        .channels = 1u,
        .encoding = cmn::meta_encoding_t::gray
    };

    constexpr inline uint8_t bgr2gray(const RGBArray& bgr) noexcept {
        return saturate(float(bgr[0]) * 0.114 + float(bgr[1]) * 0.587 + float(bgr[2]) * 0.299, 0, 255);
    }
    constexpr inline uint8_t bgr2gray(const uint8_t* bgr) noexcept {
        return saturate(float(bgr[0]) * 0.114 + float(bgr[1]) * 0.587 + float(bgr[2]) * 0.299, 0, 255);
    }

    template<InputInfo input, OutputInfo output>
    constexpr auto diffable_pixel_value(const uchar* input_data) noexcept {
        static_assert(is_in(input.channels, 1, 3), "Input channels can only be 1 or 3.");
        static_assert(is_in(input.channels, 1, 3), "Output channels can only be 1 or 3.");
        assert(not input.is_r3g3b2() || input.channels != 3); //, "Three input channels cannot be R3G3B2 encoded.");

        if constexpr(input.channels == 1) {
            if constexpr(input.is_r3g3b2()) {
                if constexpr(output.channels == 3) {
                    return r3g3b2_to_vec(*input_data);
                } else if constexpr(output.channels == 1) {
                    if constexpr(output.is_r3g3b2()) {
                        return *input_data;
                    }
                    return bgr2gray(r3g3b2_to_vec(*input_data));
                }
                
            } else if constexpr(output.channels == 3) {
                return RGBArray{*input_data, *input_data, *input_data};
                
            } else if constexpr(output.channels == 1) {
                if constexpr(output.is_r3g3b2()) {
                    return vec_to_r3g3b2(RGBArray{*input_data, *input_data, *input_data});
                } else {
                    return *input_data;
                }
            }
            
        } else if constexpr(output.channels == 3) {
            return RGBArray{*(input_data + 0), *(input_data + 1), *(input_data + 2)};
            
        } else if constexpr(output.channels == 1) {
            if constexpr(output.is_r3g3b2()) {
                return vec_to_r3g3b2(RGBArray{*(input_data + 0), *(input_data + 1), *(input_data + 2)});
            } else {
                return bgr2gray(input_data);
            }
        }
    }

template<InputInfo input, OutputInfo output>
constexpr auto dual_diffable_pixel_value(const uchar* input_data) noexcept {
    auto pixel_value = diffable_pixel_value<input, output>(input_data);

    uchar grey_value;
    if constexpr(output.channels == 3) {
        grey_value = bgr2gray(pixel_value);
    } else if constexpr(output.is_r3g3b2()) {
        grey_value = bgr2gray(r3g3b2_to_vec(pixel_value));
    } else {
        grey_value = pixel_value;
    }
    
    return std::make_tuple(pixel_value, grey_value);
}

    template<OutputInfo output, typename Pixel>
        requires (std::same_as<Pixel, RGBArray> || std::is_integral_v<Pixel>)
    constexpr void write_pixel_value(uchar* image_ptr, Pixel value) {
        //static_assert(is_in(input.channels, 3, 1), "Need input to have either 3 or 1 channels.");
        static_assert(is_in(output.channels, 3, 1), "Need output to have either 3 or 1 channels.");
        
        if constexpr(std::is_integral_v<Pixel>) {
            if constexpr(output.channels == 1) {
                *image_ptr = value;
                
            } else {
                static_assert(output.channels == 3, "Expecting 3 output channels here.");
                *image_ptr = *(image_ptr + 1) = *(image_ptr + 2) = value;
            }
            
        } else if constexpr(std::same_as<Pixel, RGBArray>) {
            if constexpr(output.channels == 3) {
                *(image_ptr + 0) = value[0];
                *(image_ptr + 1) = value[1];
                *(image_ptr + 2) = value[2];
            } else {
                static_assert(output.channels == 1, "Expecting one output channel.");
                *(image_ptr) = bgr2gray(value);
            }
        }
    }

    template<OutputInfo output, DifferenceMethod method, typename Pixel>
    struct DifferenceImpl {
        static int convert_to_grayscale(int r3g3b2) {
            if constexpr(output.is_r3g3b2()) {
                return bgr2gray(r3g3b2_to_vec<3>((uint8_t)r3g3b2));
            } else
                return r3g3b2;
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::absolute && is_rgb_array<Pixel>::value)
        inline Pixel operator()(Pixel source, Pixel value) const {
            return Pixel{
                (uchar)saturate(abs(int32_t(source[0]) - int32_t(value[0])), 0, 255),
                (uchar)saturate(abs(int32_t(source[1]) - int32_t(value[1])), 0, 255),
                (uchar)saturate(abs(int32_t(source[2]) - int32_t(value[2])), 0, 255)
            };
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::sign && is_rgb_array<Pixel>::value)
        inline Pixel operator()(Pixel source, Pixel value) const {
            return Pixel{
                (uchar)max(0, int32_t(source[0]) - int32_t(value[0])),
                (uchar)max(0, int32_t(source[1]) - int32_t(value[1])),
                (uchar)max(0, int32_t(source[2]) - int32_t(value[2]))
            };
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::none && is_rgb_array<Pixel>::value)
        inline Pixel operator()(Pixel, Pixel value) const {
            return value;
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::absolute && not is_rgb_array<Pixel>::value)
        inline int operator()(int source, int value) const {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return abs(source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::sign && not is_rgb_array<Pixel>::value)
        inline int operator()(int source, int value) const {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return max(0, source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod::none && not is_rgb_array<Pixel>::value)
        inline int operator()(int, int value) const {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
            }
            return value;
        }
    };

    class Background {
    public:
        static bool track_absolute_difference();
        static bool track_background_subtraction();
        static meta_encoding_t::Class meta_encoding();
        static ImageMode image_mode();
        
    protected:
        Image::SPtr _image, _grey_image;
        LuminanceGrid* _grid;
        Bounds _bounds;
        //int (*_diff)(int, int);
        std::string _name;
        CallbackCollection _callback;
        
    public:
        Background(Image::Ptr&& image, LuminanceGrid* grid);
        ~Background();
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        auto diff(coord_t x, coord_t y, Pixel value) const {
            if constexpr(is_rgb_array<Pixel>::value) {
                static_assert(value.size() == 3, "Need three channels.");
                auto index = (ptr_safe_t(x) + ptr_safe_t(y) * ptr_safe_t(_image->cols)) * _image->channels();
                if(_image->channels() == 1) {
                    auto grey = _image->data()[index];
                    if constexpr(output.is_r3g3b2())
                        return DifferenceImpl<output, method, Pixel>{}(r3g3b2_to_vec(grey), value);
                    else
                        return DifferenceImpl<output, method, Pixel>{}(RGBArray{grey,grey,grey}, value);
                } else {
                    assert(_image->channels() == 3);
                    return DifferenceImpl<output, method, Pixel>{}(RGBArray{
                        _image->data()[index],
                        _image->data()[index+1],
                        _image->data()[index+2]
                    }, value);
                }
                
            } else {
                return DifferenceImpl<output, method, Pixel>{}(_grey_image->data()[ptr_safe_t(x) + ptr_safe_t(y) * ptr_safe_t(_grey_image->cols)], value);
            }
        }
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        bool is_different(coord_t x, coord_t y, Pixel value, int threshold) const {
            return is_value_different<output, Pixel>(x, y, diff<output, method, Pixel>(x, y, value), threshold);
        }
        
        template<OutputInfo output, typename Pixel>
        bool is_value_different(coord_t x, coord_t y, Pixel value, int threshold) const {
            assert(x < _image->cols && y < _image->rows);
            if constexpr(is_rgb_array<Pixel>::value) {
                return bgr2gray(value) >= (_grid ? _grid->relative_threshold(x, y) : 1) * threshold;
            } else {
                if constexpr(output.is_r3g3b2()) {
                    return bgr2gray(r3g3b2_to_vec(value)) >= (_grid ? _grid->relative_threshold(x, y) : 1) * threshold;
                } else {
                    return value >= (_grid ? _grid->relative_threshold(x, y) : 1) * threshold;
                }
            }
        }
        
        template<DifferenceMethod method, InputInfo input>
        coord_t count_above_threshold(coord_t x0, coord_t x1, coord_t y, std::span<uchar> values, int threshold) const
        {
            /// we can assume 1 here since we use the *grey_image*
            constexpr ptr_safe_t bg_channels = 1;//_image->channels();
            constexpr OutputInfo output {
                .channels = 1u,
                .encoding = meta_encoding_t::gray
            };
            
            auto ptr_grid = _grid
                ? (_grid->thresholds().data()
                    + ptr_safe_t(x0) + ptr_safe_t(y) * (ptr_safe_t)_grid->bounds().width)
                : NULL;
            auto ptr_image = _grey_image->data() + (ptr_safe_t(x0) + ptr_safe_t(y) * ptr_safe_t(_grey_image->cols)) * bg_channels;
            auto ptr_values = values.data();
            auto end = values.data() + (ptr_safe_t(x1) - ptr_safe_t(x0) + 1) * input.channels;
            assert(end <= values.data() + values.size());
            ptr_safe_t count = 0;
            
            if constexpr (method == DifferenceMethod::sign) {
                if(ptr_grid) {
                    for (; ptr_values != end; ++ptr_grid, ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(threshold);
                }
                
            } else if constexpr(method == DifferenceMethod::absolute) {
                if(ptr_grid) {
                    for (; ptr_values != end; ++ptr_grid, ptr_image += bg_channels, ptr_values += input.channels)
                        count += std::abs(int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values))) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += std::abs(int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values))) >= int32_t(threshold);
                }
            } else {
                if(ptr_grid) {
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(*ptr_grid) * threshold;
                } else {
                    for (; ptr_values != end; ptr_values += input.channels)
                        count += int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(threshold);
                }
            }
            
            return count;
        }
        
        //int color(coord_t x, coord_t y) const;
        template<OutputInfo output = OutputInfo{
            .channels = 3u,
            .encoding = meta_encoding_t::rgb8
        }>
        constexpr auto color(coord_t x, coord_t y) const {
            static_assert(is_in(output.channels, 1, 3), "Channels must be either 1 or 3.");
            
            auto ptr = _image->data() + (ptr_safe_t(x) + ptr_safe_t(y) * ptr_safe_t(_image->cols)) * ptr_safe_t(_image->channels());
            
            if(_image->channels() == 1) {
                /// grayscale image
                return diffable_pixel_value<InputInfo{
                    .channels = 1u,
                    .encoding = meta_encoding_t::gray
                }, output>(ptr);
                
            } else {
                return diffable_pixel_value<InputInfo{
                    .channels = 1u,
                    .encoding = meta_encoding_t::rgb8
                }, output>(ptr);
            }
        }
        
        const Image& image() const;
        const Bounds& bounds() const;
        const LuminanceGrid* grid() const {
            return _grid;
        }
        
    private:
        void update_callback();
    };

    //! Converts a lines array to a mask or greyscale (or both).
    //  requires pixels to contain actual greyscale values
    std::pair<cv::Rect2i, size_t> imageFromLines(
         InputInfo input,
         const std::vector<HorizontalLine>& lines,
         cv::Mat* output_mask,
         cv::Mat* output_greyscale = NULL,
         cv::Mat* output_differences = NULL,
         const std::vector<uchar>* pixels = NULL,
         const int threshold = 0,
         const Background* average = NULL,
         int padding = 0);

    /*class LuminanceGrid;
    std::pair<cv::Rect2i, size_t> imageFromLines(
         InputInfo input,
         const std::vector<HorizontalLine>& lines,
         cv::Mat* output_mask,
         cv::Mat* output_greyscale,
         cv::Mat* output_differences,
         const std::vector<uchar>& pixels,
         int base_threshold,
         const LuminanceGrid& grid,
         const Background* background,
         int padding);*/

    auto determine_colors(auto&& fn) {
        auto encoding = Background::image_mode();
        if(encoding == ImageMode::R3G3B2)
            return fn.template operator()<ImageMode::R3G3B2>();
        else if(encoding == ImageMode::GRAY)
            return fn.template operator()<ImageMode::GRAY>();
        else if(encoding == ImageMode::RGB)
            return fn.template operator()<ImageMode::RGB>();
        else if(encoding == ImageMode::RGBA)
            return fn.template operator()<ImageMode::RGBA>();
        else
            throw InvalidArgumentException("Invalid color mode for image.");
    }

    auto call_image_mode_function(const auto& fn) {
        if(not Background::track_background_subtraction()) {
            return fn.template operator()<DifferenceMethod::none>();
        } else if(Background::track_absolute_difference()) {
            return fn.template operator()<DifferenceMethod::absolute>();
        } else {
            return fn.template operator()<DifferenceMethod::sign>();
        }
    }

/// when the number of output channels is known:
struct KnownOutputType {};
template<OutputInfo output>
auto call_image_mode_function(KnownOutputType, auto&& fn) {
    static_assert(is_in(output.channels, 1, 3), "Output channels need to be either 1 or 3.");
    
    if(not Background::track_background_subtraction()) {
        return fn.template operator()<output, DifferenceMethod::none>();
    } else if(Background::track_absolute_difference()) {
        return fn.template operator()<output, DifferenceMethod::absolute>();
    } else {
        return fn.template operator()<output, DifferenceMethod::sign>();
    }
}

template<OutputInfo output>
auto call_image_mode_function(InputInfo input, KnownOutputType, const auto& fn) {
    static_assert(is_in(output.channels, 1, 3), "Output channels need to be either 1 or 3.");
    auto determine_in = [&]<OutputInfo, DifferenceMethod method>() {
        return call_single_image_info(input, [&]<InputInfo i>(){
            return fn.template operator()<i, output, method>();
        });
    };
    
    call_image_mode_function<output>(KnownOutputType{}, determine_in);
}

template<typename Info>
constexpr auto call_single_image_info(Info info, const auto& fn) {
    auto determine_encoding = [&]<uint8_t channels>() {
        switch(info.encoding) {
            case meta_encoding_t::data::values::r3g3b2: {
                if constexpr(channels == 3)
                    throw InvalidArgumentException("Invalid number of channels for R3G3B2 encoding: ", channels);
                
                return fn.template operator()<Info{
                    .channels = channels,
                    .encoding = meta_encoding_t::r3g3b2
                }>();
            }
            case meta_encoding_t::data::values::gray: {
                return fn.template operator()<Info{
                    .channels = channels,
                    .encoding = meta_encoding_t::gray
                }>();
            }
            case meta_encoding_t::data::values::rgb8: {
                return fn.template operator()<Info{
                    .channels = channels,
                    .encoding = meta_encoding_t::rgb8
                }>();
            }
                
            default:
                throw InvalidArgumentException("Unknown encoding type: ", info.encoding);
        }
    };
    
    if(info.channels == 1)
        return determine_encoding.template operator()<1>();
    else if(info.channels == 3)
        return determine_encoding.template operator()<3>();
    else
        throw InvalidArgumentException("Invalid number of output channels: ", info);
}

    auto call_image_mode_function(OutputInfo o, auto&& fn) {
        return call_single_image_info(o, [&]<OutputInfo output>{
            if(not Background::track_background_subtraction()) {
                return fn.template operator()<output, DifferenceMethod::none>();
            } else if(Background::track_absolute_difference()) {
                return fn.template operator()<output, DifferenceMethod::absolute>();
            } else {
                return fn.template operator()<output, DifferenceMethod::sign>();
            }
        });
    }

    auto call_image_mode_function(InputInfo input, OutputInfo output, const auto& fn) {
        auto determine_in = [&]<OutputInfo o, DifferenceMethod method>() {
            return call_single_image_info(input, [&]<InputInfo i>(){
                return fn.template operator()<i, o, method>();
            });
        };
        
        return call_image_mode_function(output, determine_in);
    }

}
