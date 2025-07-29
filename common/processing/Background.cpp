#include "Background.h"
#include <misc/GlobalSettings.h>

namespace cmn {
    static std::atomic<bool> track_threshold_is_absolute = true,
        track_background_subtraction = true;
    static std::atomic<meta_encoding_t::data::values> meta_encoding = cmn::meta_encoding_t::gray.value();

    static auto check_callbacks = []() {
        static std::once_flag _check_callbacks;
        std::call_once(_check_callbacks, []() {
            auto _callback = GlobalSettings::register_callbacks({
                "track_threshold_is_absolute",
                "track_background_subtraction",
                "meta_encoding"

            }, [](std::string_view name) {
                if (name == "track_threshold_is_absolute") {
                    cmn::track_threshold_is_absolute = SETTING(track_threshold_is_absolute).value<bool>();
                }
                else if (name == "track_background_subtraction") {
                    cmn::track_background_subtraction = SETTING(track_background_subtraction).value<bool>();
                }
                else if (name == "meta_encoding") {
                    cmn::meta_encoding = SETTING(meta_encoding).value<meta_encoding_t::Class>().value();

                    //Print("updated meta_encoding to ", meta_encoding_t::Class(cmn::meta_encoding.load()));
                }
                //Background::update_callback();
            });
		});
    };

    bool Background::track_threshold_is_absolute() {
        check_callbacks();
        return cmn::track_threshold_is_absolute;
    }
    bool Background::track_background_subtraction() {
        check_callbacks();
        return //cmn::meta_encoding != meta_encoding_t::rgb8 &&
                 cmn::track_background_subtraction;
    }
    meta_encoding_t::Class Background::meta_encoding() {
        check_callbacks();
        return cmn::meta_encoding_t::Class{cmn::meta_encoding};
    }
    ImageMode Background::image_mode() {
        check_callbacks();
        return is_in(cmn::meta_encoding, meta_encoding_t::gray, cmn::meta_encoding_t::binary)
            ? ImageMode::GRAY
            : (cmn::meta_encoding == meta_encoding_t::rgb8
                ? ImageMode::RGB
                : ImageMode::R3G3B2);
    }

Background::Background(Size2 dimensions, meta_encoding_t::Class)
    : _bounds(Vec2(), dimensions)
{
    
}

    Background::Background(Image::Ptr&& image, meta_encoding_t::Class encoding)
        : _image(std::move(image)), /*_grid(grid),*/ _bounds(_image->bounds())
    {
        if(encoding == meta_encoding_t::r3g3b2) {
            auto ptr = Image::Make(_image->rows, _image->cols, 3);
            cv::Mat output = ptr->get();
            convert_from_r3g3b2(_image->get(), output);
            _image = std::move(ptr);
            
        } else if(encoding == meta_encoding_t::gray) {
            auto ptr = Image::Make(_image->rows, _image->cols, 3);
            cv::Mat output = ptr->get();
            cv::cvtColor(_image->get(), output, cv::COLOR_GRAY2BGR);
            _image = std::move(ptr);
        }
        
        if(_image->channels() == 3) {
            _grey_image = Image::Make(_image->rows, _image->cols, 1);
            cv::cvtColor(_image->get(), _grey_image->get(), cv::COLOR_BGR2GRAY);
        } else {
            _grey_image = _image;
        }
    }
    
    Background::~Background() {
        if(_callback)
            GlobalSettings::unregister_callbacks(std::move(_callback));
    }

    void Background::update_callback() {
#ifndef NDEBUG
        if(GlobalSettings::is_runtime_quiet())
            Print("Updating static background difference method.");
#endif
    }
    
    const Image& Background::image() const {
        return *_image;
    }
    
    const Bounds& Background::bounds() const {
        return _bounds;
    }

inline void _lines_initialize_matrix(cv::Mat& mat, int w, int h, int type = CV_8UC1) {
    if (mat.empty() || mat.type() != type || mat.rows != h || mat.cols != w)
        mat = cv::Mat::zeros(h, w, type);
    else
        mat = cv::Scalar(0);
};

std::pair<cv::Rect2i, size_t> imageFromLines(InputInfo input,
                                             const std::vector<HorizontalLine>& lines,
                                             cv::Mat* output_mask,
                                             cv::Mat* output_greyscale,
                                             cv::Mat* output_differences,
                                             const PixelArray_t* pixels,
                                             const int base_threshold,
                                             const Background* background,
                                             int padding)
{
#ifndef NDEBUG
    if(not is_in(input.channels, 0, 1, 3)) {
        throw InvalidArgumentException("Invalid number of channels (",input,") in imageFromLines.");
    }
#endif
    
    auto r = lines_dimensions(lines);
    r.x -= padding;
    r.y -= padding;
    r.width += padding * 2;
    r.height += padding * 2;
    
    OutputInfo output{
        .channels = static_cast<uint8_t>(input.encoding == meta_encoding_t::gray ? 1 : 3),
        .encoding = input.encoding == meta_encoding_t::gray
                        ? meta_encoding_t::gray
                        : meta_encoding_t::rgb8
    };
    
    // initialize matrices
    if(output_mask)
        _lines_initialize_matrix(*output_mask, r.width, r.height);
    if(output_greyscale)
        _lines_initialize_matrix(*output_greyscale, r.width, r.height, CV_8UC(output.channels));
    if(output_differences)
        _lines_initialize_matrix(*output_differences, r.width, r.height, CV_8UC(output.channels));
    
    size_t recount = 0;
    auto pixels_ptr = pixels ? pixels->data() : nullptr;
    
    auto work_pixels = [&]<bool has_image, bool has_differences, bool has_pixels, 
                           InputInfo input, OutputInfo output, DifferenceMethod method>()
    {
        using value_t = decltype(diffable_pixel_value<input, output>(pixels_ptr));
        value_t value, diff;
        
        for (auto &l : lines) {
            for (int x=l.x0; x<=l.x1; x++, pixels_ptr += input.channels) {
                bool pixel_is_set = base_threshold == 0;
                
                if constexpr(input.channels == 0) {
                    pixel_is_set = true;
                    
                    if constexpr(is_rgb_array<value_t>::value) {
                        value = value_t{ 255, 255, 255 };
                        diff = value;
                    } else {
                        value = 255;
                        diff = 255;
                    }
                    
                } else if constexpr(has_pixels) {
                    if(base_threshold > 0 || has_image || has_differences) {
                        value = diffable_pixel_value<input, output>(pixels_ptr);
                    }
                    if((not pixel_is_set && base_threshold > 0) || has_differences) {
                        diff = background->diff<output, method>(x, l.y, value);
                    }
                    
                    pixel_is_set = pixel_is_set || background->is_value_different<output>(x, l.y, diff, base_threshold);
                }
                
                if(pixel_is_set) {
                    if(output_mask)
                        output_mask->at<uchar>(l.y - r.y, x - r.x) = 255;
                    
                    if constexpr(has_pixels) {
                        if constexpr(output.channels == 3)
                        {
                            if constexpr(has_image) {
                                assert(output_greyscale->channels() == 3);
                                output_greyscale->at<cv::Vec3b>(l.y - r.y, x - r.x) = cv::Vec3b(value[0], value[1], value[2]);
                            }
                            
                            if constexpr(has_differences)
                                output_differences->at<cv::Vec3b>(l.y - r.y, x - r.x) = cv::Vec3b(diff[0], diff[1], diff[2]);
                            
                        } else if constexpr(output.channels == 1) {
                            if constexpr(has_image) {
                                if constexpr(input.channels == 1) {
                                    output_greyscale->at<uchar>(l.y - r.y, x - r.x) = value;
                                    
                                } else if constexpr(input.channels == 3) {
                                    if constexpr(is_rgb_array<decltype(value)>::value) {
                                        auto grey_value = bgr2gray(value);
                                        output_greyscale->at<uchar>(l.y - r.y, x - r.x) = grey_value;
                                    } else {
                                        output_greyscale->at<uchar>(l.y - r.y, x - r.x) = value;
                                    }
                                }
                            }
                            
                            if constexpr(has_differences)
                                output_differences->at<uchar>(l.y - r.y, x - r.x) = diff;
                        }
                    }
                    
                    recount++;
                }
            }
        }
    };
    
    auto work_image = [&]<bool has_differences, bool has_pixels, InputInfo input, OutputInfo output, DifferenceMethod method>() {
        if(output_greyscale) {
            if constexpr(has_pixels) {
                return work_pixels.template operator()<true, has_differences, has_pixels, input, output, method>();
            } else {
                throw InvalidArgumentException("Cannot output images without pixels.");
            }
            
        } else {
            return work_pixels.template operator()<false, has_differences, has_pixels, input, output, method>();
        }
    };
    
    auto work_threshold = [&]<bool has_pixels, InputInfo input, OutputInfo output, DifferenceMethod method>() {
        if(output_differences) {
            if constexpr(input.channels == 0) {
                return work_image.template operator()<true, has_pixels, input, output, DifferenceMethod_t::none>();
            } else if constexpr(has_pixels) {
                return work_image.template operator()<true, has_pixels, input, output, method>();
            } else {
                throw InvalidArgumentException("Cannot output differences without pixels.");
            }
            
        } else {
            return work_image.template operator()<false, has_pixels, input, output, method>();
        }
    };
    
    auto work = [&]<InputInfo input, OutputInfo output, DifferenceMethod method>() {
        static_assert(is_in(input.channels, 0, 1, 3), "Only 0, 1 or 3 channels input is supported.");
        static_assert(is_in(output.channels, 1,3), "Only 1 or 3 channels output is supported.");
        
        if constexpr(input.channels == 0) {
            /*if(output_differences)
                throw InvalidArgumentException("Cannot output differences without providing pixels.");
            if(output_greyscale)
                throw InvalidArgumentException("Cannot output images without pixels.");*/
            
            return work_threshold.template operator()<true, input, output, DifferenceMethod_t::none>();
            
        } else if(pixels) {
            return work_threshold.template operator()<true, input, output, method>();
            
        } else {
            if(output_differences)
                throw InvalidArgumentException("Cannot output differences without providing pixels.");
            if(output_greyscale)
                throw InvalidArgumentException("Cannot output images without pixels.");
            
            return work_threshold.template operator()<false, input, output, method>();
        }
    };
    
    call_image_mode_function(input, output, work);
    
    return {r, recount};
}

/*std::pair<cv::Rect2i, size_t> imageFromLines(InputInfo input, const std::vector<HorizontalLine>& lines, cv::Mat* output_mask, cv::Mat* output_greyscale, cv::Mat* output_differences, const PixelArray_t* input_pixels, const int threshold, const Background* average, int padding)
{
#ifndef NDEBUG
    if(not is_in(input.channels, 1, 3)) {
        throw InvalidArgumentException("Invalid number of channels (",input,") in imageFromLines.");
    }
#endif
    assert(not output_differences || average);
    
    auto r = lines_dimensions(lines);
    r.x -= padding;
    r.y -= padding;
    r.width += padding * 2;
    r.height += padding * 2;
    
    // initialize matrices
    if(output_mask)
        _lines_initialize_matrix(*output_mask, r.width, r.height);
    if(output_greyscale)
        _lines_initialize_matrix(*output_greyscale, r.width, r.height, CV_8UC(input.channels));
    if(output_differences) {
        _lines_initialize_matrix(*output_differences, r.width, r.height, CV_8UC(input.channels));
    }
    
    uint32_t pos = 0;
    size_t recount = 0;
    
    // use individual ifs for cases to have less
    // jumps in assembler code within the loops
    if(threshold == 0) {
        int c;
        for (auto &l : lines) {
            recount += size_t(l.x1) - size_t(l.x0) + 1;
            
            for (int x=l.x0; x<=l.x1; x++) {
                if(output_mask)
                    output_mask->at<uchar>(l.y - r.y, x - r.x) = 255;
                if(output_greyscale || output_differences) {
                    c = input_pixels->at(pos * input.channels);
                    
                    if(output_greyscale) {
                        if(input.channels == 1)
                            output_greyscale->at<uchar>(l.y - r.y, x - r.x) = c;
                        else if(input.channels == 3)
                            output_greyscale->at<cv::Vec3b>(l.y - r.y, x - r.x) = *((const cv::Vec3b*)input_pixels->data() + pos);
                    }
                    
                    /// TODO: need to account for number of channels here
                    if(output_differences)
                        output_differences->at<uchar>(l.y - r.y, x - r.x) = min(UCHAR_MAX, cmn::abs(int(average->at(l.y,x)) - c));
                        //min(UCHAR_MAX, max(0, int(average->at<uchar>(l.y, x)) - c));
                    
                    ++pos;
                }
            }
        }
        
    } else {
        int c, diff;
        assert(average);
        assert(threshold);
        assert(input_pixels);
        
        auto pixels_ptr = input_pixels->data();
        for (auto &l : lines) {
            for (int x=l.x0; x<=l.x1; x++, pixels_ptr += input.channels) {
                c = *pixels_ptr;
                
                /// TODO: need to account for number of channels here
                diff = min(UCHAR_MAX, cmn::abs(int(average->at(l.y,x)) - c));
                //diff = min(UCHAR_MAX, max(0, int(average->at<uchar>(l.y, x)) - c));
                
                if(diff >= threshold) {
                    if(output_mask)
                        output_mask->at<uchar>(l.y - r.y, x - r.x) = 255;
                    
                    if(output_greyscale) {
                        if(input.channels == 1)
                            output_greyscale->at<uchar>(l.y - r.y, x - r.x) = c;
                        else if(input.channels == 3)
                            output_greyscale->at<cv::Vec3b>(l.y - r.y, x - r.x) = *((const cv::Vec3b*)pixels_ptr);
                    }
                    
                    if(output_differences)
                        output_differences->at<uchar>(l.y - r.y, x - r.x) = diff;
                    
                    recount++;
                }
            }
        }
        
    }
    
    return {r, recount};
}*/

}
