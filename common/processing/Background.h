#pragma once

#include <commons.pc.h>
#include <misc/Image.h>
//#include <processing/LuminanceGrid.h>
#include <processing/encoding.h>

namespace cmn {

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
    constexpr bool is_binary() const { \
        return meta_encoding_t::binary == encoding; \
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
        return saturate(float(bgr[0]) * 0.114 + float(bgr[1]) * 0.587 + float(bgr[2]) * 0.299 + 0.5, 0, 255);
    }
    constexpr inline uint8_t bgr2gray(const uint8_t* bgr) noexcept {
        return saturate(float(bgr[0]) * 0.114 + float(bgr[1]) * 0.587 + float(bgr[2]) * 0.299 + 0.5, 0, 255);
    }

    template<InputInfo input, OutputInfo output>
    constexpr auto diffable_pixel_value(const uchar* input_data)
#ifdef NDEBUG
        noexcept
#endif
{
        static_assert(is_in(input.channels, 0, 1, 3), "Input channels can only be 0, 1 or 3.");
        static_assert(is_in(output.channels, 1, 3), "Output channels can only be 1 or 3.");
#ifndef NDEBUG
    //static_assert(input.channels == 0 || not input.is_binary(), "");
        if(not input.is_binary() || input.channels == 0) {
            
        } else {
            throw InvalidArgumentException("input=",input);
        }
#endif
        //assert(not input.is_r3g3b2() || input.channels != 3); //, "Three input channels cannot be R3G3B2 encoded.");

        if constexpr(input.channels == 0) {
            if constexpr(output.channels == 3) {
                return RGBArray{255, 255, 255};
                
            } else if constexpr(output.channels == 1) {
                if constexpr(output.is_r3g3b2()) {
                    return vec_to_r3g3b2(RGBArray{255, 255, 255});
                } else {
                    return 255;
                }
            }
            
        } else if constexpr(input.channels == 1) {
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
using PixelOutput_t = decltype(diffable_pixel_value<input, output>(std::declval<const uchar*>()));

template<typename Impl, typename... Args>
concept has_static_apply = requires(Args&&... args) {
    { Impl::apply(std::forward<Args>(args)...) };
};

template<InputInfo input, OutputInfo output, typename Pixel>
constexpr uchar to_grey_value(Pixel pixel_value) noexcept {
    if constexpr(output.channels == 3) {
        return bgr2gray(pixel_value);
    } else if constexpr(output.is_r3g3b2()) {
        return bgr2gray(r3g3b2_to_vec(pixel_value));
    } else {
        return pixel_value;
    }
}

template<InputInfo input, OutputInfo output>
constexpr uchar grey_diffable_pixel_value(const uchar* input_data) noexcept {
    auto pixel_value = diffable_pixel_value<input, output>(input_data);
    return to_grey_value<input, output>(pixel_value);
}

template<InputInfo input, OutputInfo output>
constexpr auto dual_diffable_pixel_value(const uchar* input_data) noexcept {
    auto pixel_value = diffable_pixel_value<input, output>(input_data);
    return std::make_tuple(pixel_value, to_grey_value<input, output>(pixel_value));
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

template<OutputInfo output, typename Pixel>
    requires (std::same_as<Pixel, RGBArray> || std::is_integral_v<Pixel>)
constexpr void push_pixel_value(PixelArray_t& image_ptr, Pixel value) {
    //static_assert(is_in(input.channels, 3, 1), "Need input to have either 3 or 1 channels.");
    static_assert(is_in(output.channels, 3, 1), "Need output to have either 3 or 1 channels.");
    
    if constexpr(std::is_integral_v<Pixel>) {
        if constexpr(output.channels == 1) {
            image_ptr.push_back(value);
            
        } else {
            static_assert(output.channels == 3, "Expecting 3 output channels here.");
            image_ptr.insert(image_ptr.end(), 3u, value);
        }
        
    } else if constexpr(std::same_as<Pixel, RGBArray>) {
        if constexpr(output.channels == 3) {
            //image_ptr.insert(image_ptr.end(), {value[0], value[1], value[2]});
            image_ptr.push_back(value[0], value[1], value[2]);
        } else {
            static_assert(output.channels == 1, "Expecting one output channel.");
            image_ptr.push_back(bgr2gray(value));
        }
    }
}

    template<OutputInfo output, DifferenceMethod method, typename Pixel>
    struct DifferenceImpl {
        static constexpr int convert_to_grayscale(int r3g3b2) {
            if constexpr(output.is_r3g3b2()) {
                return bgr2gray(r3g3b2_to_vec<3>((uint8_t)r3g3b2));
            } else
                return r3g3b2;
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::absolute && is_rgb_array<Pixel>::value)
        static constexpr Pixel apply(Pixel source, Pixel value) {
            return Pixel{
                (uchar)saturate(abs(int32_t(source[0]) - int32_t(value[0])), 0, 255),
                (uchar)saturate(abs(int32_t(source[1]) - int32_t(value[1])), 0, 255),
                (uchar)saturate(abs(int32_t(source[2]) - int32_t(value[2])), 0, 255)
            };
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::sign && is_rgb_array<Pixel>::value)
        static constexpr Pixel apply(Pixel source, Pixel value) {
            return Pixel{
                (uchar)max(0, int32_t(source[0]) - int32_t(value[0])),
                (uchar)max(0, int32_t(source[1]) - int32_t(value[1])),
                (uchar)max(0, int32_t(source[2]) - int32_t(value[2]))
            };
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::none && is_rgb_array<Pixel>::value)
        static constexpr Pixel apply(Pixel value) {
            return value;
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::absolute && not is_rgb_array<Pixel>::value)
        static constexpr int apply(int source, int value) {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return std::abs(source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::sign && not is_rgb_array<Pixel>::value)
        static constexpr int apply(int source, int value) {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
                source = convert_to_grayscale(source);
            }
            return max(0, source - value);
        }
        
        template<DifferenceMethod M = method>
            requires (M == DifferenceMethod_t::none && not is_rgb_array<Pixel>::value)
        static constexpr int apply(int value) {
            if constexpr(output.is_r3g3b2()) {
                value = convert_to_grayscale(value);
            }
            return value;
        }
    };

    class Background {
    public:
        static bool track_threshold_is_absolute();
        static bool track_background_subtraction();
        static meta_encoding_t::Class meta_encoding();
        static ImageMode image_mode();
        
    protected:
        Image::SPtr _image, _grey_image;
        //LuminanceGrid* _grid;
        Bounds _bounds;
        //int (*_diff)(int, int);
        std::string _name;
        cmn::CallbackFuture _callback;
        
    public:
        /// create a dummy background with no image
        /// supposed to be used in cases of no background subtraction for example
        Background(Size2, meta_encoding_t::Class encoding);
        
        /// create a real background with an image
        Background(Image::Ptr&& image, meta_encoding_t::Class encoding);
        ~Background();
        
        struct BackgroundInfo {
            const uchar* data;
            ptr_safe_t width;
            ptr_safe_t channels;
        };
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        static auto diff(coord_t x, coord_t y, BackgroundInfo info, Pixel value) {
            using impl = DifferenceImpl<output, method, Pixel>;
            
            if constexpr(is_rgb_array<Pixel>::value) {
                static_assert(value.size() == 3, "Need three channels.");
                
                if constexpr(has_static_apply<impl, RGBArray, RGBArray>) {
                    assert(info.data != nullptr);
                    const ptr_safe_t index = (ptr_safe_t(x) + ptr_safe_t(y) * info.width) * info.channels;
                    
                    if(info.channels == 1) {
                        auto grey = info.data[index];
                        if constexpr(output.is_r3g3b2())
                            return impl::apply(r3g3b2_to_vec(grey), value);
                        else
                            return impl::apply(RGBArray{grey, grey, grey}, value);
                    } else {
                        assert(info.channels == 3);
                        return impl::apply(RGBArray{
                            info.data[index],
                            info.data[index+1],
                            info.data[index+2]
                        }, value);
                    }
                    
                } else {
                    /*if(info.channels == 1) {
                        if constexpr(output.is_r3g3b2())
                            return impl::apply(value);
                        else
                            return impl::apply(value);
                    } else {*/
                        //assert(info.channels == 3);
                        return impl::apply(value);
                    //}
                }
                
            } else {
                if constexpr(has_static_apply<impl, int, int>) { //std::is_invocable_v<decltype(&impl::apply), int, int>) {
                    assert(info.data != nullptr);
                    const ptr_safe_t index = (ptr_safe_t(x) + ptr_safe_t(y) * info.width) * info.channels;
                    return impl::apply(info.data[index], value);
                } else {
                    return impl::apply(value);
                }
            }
        }
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        inline BackgroundInfo info() const {
            using impl = DifferenceImpl<output, method, Pixel>;
            
            if constexpr(is_rgb_array<Pixel>::value) {
                if constexpr(has_static_apply<impl, RGBArray, RGBArray>) {
                    const Image* image = _image.get();
                    return BackgroundInfo{
                        .data = image->data(),
                        .width = ptr_safe_t(image->cols),
                        .channels = image->channels()
                    };
                }
                
            } else if constexpr(has_static_apply<impl, int, int>) {
                const Image* image = _grey_image.get();
                return BackgroundInfo{
                    .data = image->data(),
                    .width = ptr_safe_t(image->cols),
                    .channels = image->channels()
                };
            }
            
            return BackgroundInfo{
                .data = nullptr,
                .width = 0,
                .channels = 0
            };
        }
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        inline auto diff(coord_t x, coord_t y, Pixel value) const {
            return diff<output, method, Pixel>(x, y, info<output, method, Pixel>(), value);
        }
        
        template<OutputInfo output, DifferenceMethod method, typename Pixel>
        constexpr inline bool is_different(coord_t x, coord_t y, Pixel value, int threshold, BackgroundInfo i) const {
            return is_value_different<output, Pixel>(x, y, diff<output, method, Pixel>(x, y, i, value), threshold);
        }
        
        template<OutputInfo output, typename Pixel>
        constexpr inline bool is_value_different([[maybe_unused]] coord_t x, [[maybe_unused]] coord_t y, Pixel value, int threshold) const {
            assert(x < _image->cols && y < _image->rows);
            if constexpr(is_rgb_array<Pixel>::value) {
                return bgr2gray(value) >= /*(_grid ? _grid->relative_threshold(x, y) : 1) * */threshold;
            } else {
                if constexpr(output.is_r3g3b2()) {
                    return bgr2gray(r3g3b2_to_vec(value)) >= /*(_grid ? _grid->relative_threshold(x, y) : 1) **/ threshold;
                } else {
                    return value >= /*(_grid ? _grid->relative_threshold(x, y) : 1) **/ threshold;
                }
            }
        }
        
        template<DifferenceMethod method, InputInfo input>
        coord_t count_above_threshold(coord_t x0, coord_t x1, coord_t y, std::span<uchar> values, int threshold) const
        {
            /// we can assume 1 here since we use the *grey_image*
            static constexpr ptr_safe_t bg_channels = 1;//_image->channels();
            static constexpr OutputInfo output {
                .channels = 1u,
                .encoding = meta_encoding_t::gray
            };
            
            /*auto ptr_grid = _grid
                ? (_grid->thresholds().data()
                    + ptr_safe_t(x0) + ptr_safe_t(y) * (ptr_safe_t)_grid->bounds().width)
                : NULL;*/
            auto ptr_image = not _grey_image ? nullptr : (_grey_image->data() + (ptr_safe_t(x0) + ptr_safe_t(y) * ptr_safe_t(_grey_image->cols)) * bg_channels);
            auto ptr_values = values.data();
            auto end = values.data() + (ptr_safe_t(x1) - ptr_safe_t(x0) + 1) * input.channels;
            assert(end <= values.data() + values.size());
            ptr_safe_t count = 0;
            
            if constexpr (method == DifferenceMethod_t::sign)
            {
                assert(ptr_image != nullptr);
                /*if(ptr_grid) {
                    for (; ptr_values != end; ++ptr_grid, ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(*ptr_grid) * threshold;
                } else {*/
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(threshold);
                //}
                
            } else if constexpr(method == DifferenceMethod_t::absolute)
            {
                assert(ptr_image != nullptr);
                /*if(ptr_grid) {
                    for (; ptr_values != end; ++ptr_grid, ptr_image += bg_channels, ptr_values += input.channels)
                        count += std::abs(int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values))) >= int32_t(*ptr_grid) * threshold;
                } else {*/
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += std::abs(int32_t(*ptr_image) - int32_t(diffable_pixel_value<input, output>(ptr_values))) >= int32_t(threshold);
                //}
            } else if constexpr(method == DifferenceMethod_t::none) {
                /*if(ptr_grid) {
                    for (; ptr_values != end; ptr_image += bg_channels, ptr_values += input.channels)
                        count += int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(*ptr_grid) * threshold;
                } else {*/
                    for (; ptr_values != end; ptr_values += input.channels)
                        count += int32_t(diffable_pixel_value<input, output>(ptr_values)) >= int32_t(threshold);
                //}
            } else {
                static_assert(method != method, "Unknown difference method used.");
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
        /*const LuminanceGrid* grid() const {
            return _grid;
        }*/
        
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
         const PixelArray_t* pixels = NULL,
         const int threshold = 0,
         const Background* average = NULL,
         int padding = 0);

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
            return fn.template operator()<DifferenceMethod_t::none>();
        } else if(Background::track_threshold_is_absolute()) {
            return fn.template operator()<DifferenceMethod_t::absolute>();
        } else {
            return fn.template operator()<DifferenceMethod_t::sign>();
        }
    }

/// when the number of output channels is known:
struct KnownOutputType {};
template<OutputInfo output>
auto call_image_mode_function(KnownOutputType, auto&& fn) {
    static_assert(is_in(output.channels, 0, 1, 3), "Output channels need to be either 1 or 3.");
    
    if constexpr(output.channels == 0) {
#ifndef NDEBUG
        if(Background::track_background_subtraction()) {
            FormatWarning("Background subtraction is enabled, but we have no color data.");
        }
#endif
        return fn.template operator()<output, DifferenceMethod_t::none>();
        
    } else {
        if(not Background::track_background_subtraction()) {
            return fn.template operator()<output, DifferenceMethod_t::none>();
        } else if(Background::track_threshold_is_absolute()) {
            return fn.template operator()<output, DifferenceMethod_t::absolute>();
        } else {
            return fn.template operator()<output, DifferenceMethod_t::sign>();
        }
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
                if constexpr(channels != 1)
                    FormatWarning("Invalid number of channels for R3G3B2 encoding: ", channels);
                
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
            case meta_encoding_t::data::values::binary: {
                if constexpr(std::same_as<InputInfo, Info>) {
                    if constexpr(channels != 0)
                        throw InvalidArgumentException("Binary requires only one channel.");
                    
                    return fn.template operator()<Info{
                        .channels = channels,
                        .encoding = meta_encoding_t::binary
                    }>();
                }
            }
                
            default:
                throw InvalidArgumentException("Unknown encoding type: ", info.encoding);
        }
    };
    
    if constexpr(std::same_as<InputInfo, Info>) {
        if(info.channels == 0)
            return determine_encoding.template operator()<0>();
        else if(info.channels == 1)
            return determine_encoding.template operator()<1>();
        else if(info.channels == 3)
            return determine_encoding.template operator()<3>();
        else
            throw InvalidArgumentException("Invalid number of output channels: ", info);
    } else {
        if(info.channels == 1)
            return determine_encoding.template operator()<1>();
        else if(info.channels == 3)
            return determine_encoding.template operator()<3>();
        else
            throw InvalidArgumentException("Invalid number of output channels: ", info);
    }
}

    auto call_image_mode_function(OutputInfo o, auto&& fn) {
        return call_single_image_info(o, [&]<OutputInfo output>{
            if(not Background::track_background_subtraction()) {
                return fn.template operator()<output, DifferenceMethod_t::none>();
            } else if(Background::track_threshold_is_absolute()) {
                return fn.template operator()<output, DifferenceMethod_t::absolute>();
            } else {
                return fn.template operator()<output, DifferenceMethod_t::sign>();
            }
        });
    }

    auto call_image_mode_function(InputInfo input, OutputInfo output, const auto& fn) {
        auto determine_in = [&]<OutputInfo o, DifferenceMethod method>() {
            return call_single_image_info(input, [&]<InputInfo i>(){
                if constexpr(i.channels == 0) {
                    /// we force the difference method if we have no color input
                    /// channels. otherwise, this doesn't make sense. you cannot
                    /// build a difference without colors.
                    return fn.template operator()<i, o, DifferenceMethod_t::none>();
                } else {
                    return fn.template operator()<i, o, method>();
                }
            });
        };
        
        return call_image_mode_function(output, determine_in);
    }

}
