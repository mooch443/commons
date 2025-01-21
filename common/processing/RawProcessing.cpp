#include "RawProcessing.h"
#include "CPULabeling.h"
#include "misc/GlobalSettings.h"
#include "misc/Timer.h"
#include <misc/ocl.h>
#include <processing/LuminanceGrid.h>
#include <misc/ranges.h>
#include <processing/PadImage.h>
#include <misc/ThreadPool.h>
#include <misc/colors.h>
#include <misc/PVBlob.h>
#include <gui/DrawCVBase.h>
#include <gui/Graph.h>

namespace cmn {

gpuMat gpu_dilation_element;
std::shared_mutex mutex;

GenericThreadPool _contour_pool(5, "contour_pool");

template<typename Calc = float, typename A = uchar, typename B = uchar>
class SubtractConvert : public cv::ParallelLoopBody
{
private:
    const A * p0;
    const B * p1;
    uchar *out;
    
public:
    SubtractConvert(const A* ptr0, const B* ptr1, uchar* output ) : p0(ptr0), p1(ptr1), out(output) {}
    virtual void operator()( const cv::Range &r ) const {
        for ( int i = r.start; i != r.end; ++i)
            out[i] = cv::saturate_cast<uchar>(abs(Calc(p0[i]) - Calc(p1[i])));
    }
};

template<typename Iterator>
void process_tags(int32_t index,
                  Iterator start,
                  Iterator end,
                  cv::Mat& result,
                  TagCache* cache,
                  const gpuMat* INPUT,
                  const gpuMat& input,
                  const gpuMat* _average)
{
    using namespace gui;
    
    static const double cm_per_pixel = SETTING(cm_per_pixel).value<Float2_t>() <= 0 ? 234_F / 3007_F : SETTING(cm_per_pixel).value<Float2_t>();
    static const Range<double> tag_size_range = SETTING(tags_size_range).value<Range<double>>();
    static const auto tags_num_sides = SETTING(tags_num_sides).value<Range<int>>();
    static const auto tags_approximation = SETTING(tags_approximation).value<float>();
    static const auto tags_maximum_image_size = SETTING(tags_maximum_image_size).value<Size2>();
    
    static const bool show_debug_info = SETTING(tags_debug).value<bool>();
    
    cv::Mat l;
    cv::Mat rotated;
    cv::Mat tmp;
    std::vector<pv::BlobPtr> blobs;

    for (auto it = start; it != end; ++it, ++index) {
        auto& c = *it;
        const auto& h = cache->hierarchy[index];

        // find shapes that are inside other shapes
        // h[2]: first child. required to have one (continue if -1)
        // h[3]: parent. required to have none (continue if -1)
        if (c.size() < 2 || (h[2] == -1 && h[3] != -1)) {
            if (show_debug_info)
                cv::drawContours(result, cache->contours, index, Color(50, 0, 0, 255));
            continue;
        }

        // calculate perimeter and "approximate" / simplify the shape
        // that was found:
        auto perimeter = cv::arcLength(c, true);
        cv::approxPolyDP(cv::Mat(c), c, tags_approximation * perimeter, true);

        // BIT: tags_num_sides
        // now check if the number of sides (lines) in the shape is within
        // acceptable number of sides
        if (!tags_num_sides.contains(narrow_cast<int32_t>(c.size()))) {
            if (show_debug_info) {
                cv::drawContours(result, cache->contours, index, Color(50, 50, 0, 255));

                int min_x = INPUT->cols, min_y = INPUT->rows,
                    max_x = 0, max_y = 0;

                for (size_t j = 0; j < c.size(); ++j) {
                    auto& pt = c[j];
                    min_x = min(pt[0], min_x);
                    max_x = max(pt[0], max_x);
                    min_y = min(pt[1], min_y);
                    max_y = max(pt[1], max_y);
                }
                
                cv::putText(result, cmn::format<FormatterType::NONE>("sides: ", c.size()), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Red);
            }
            continue;
        }

        // check the contour area to exclude too large/small shapes
        auto area = cv::contourArea(c, false);
        if (area == 0) {
            continue;
        }

        // contourArea returns px^2, convert that to
        // sqrt(px^2) * cm_per_pixel => cm
        area = sqrt(area) * cm_per_pixel;

        int min_x = INPUT->cols, min_y = INPUT->rows,
            max_x = 0, max_y = 0;

        for (size_t j = 0; j < c.size(); ++j) {
            auto& pt = c[j];
            min_x = min(pt[0], min_x);
            max_x = max(pt[0], max_x);
            min_y = min(pt[1], min_y);
            max_y = max(pt[1], max_y);
        }

        // BIT: tag_area_range
        // See if the area is too big or small
        if (tag_size_range.contains(area)) {
            if (show_debug_info) {
                cv::line(result, Vec2(min_x, min_y), Vec2(max_x, min_y), Cyan, 2);
                cv::line(result, Vec2(max_x, min_y), Vec2(max_x, max_y), Cyan, 2);
                cv::line(result, Vec2(max_x, max_y), Vec2(min_x, max_y), Cyan, 2);
                cv::line(result, Vec2(min_x, max_y), Vec2(min_x, min_y), Cyan, 2);

                cv::putText(result, cmn::format<FormatterType::NONE>("area: ", area, " sides: ", c.size()), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Cyan);
                cv::drawContours(result, cache->contours, index, Color(255, 150, 0, 255));
            }

            //! add padding to Bounds and restrict to certain image size
            auto add_padding = [](Bounds bds, float padding, const Size2& image_size) -> Bounds {
                Bounds b(bds.x - padding, bds.y - padding, bds.width + padding * 2, bds.height + padding * 2);
                b.restrict_to(Bounds(Vec2(0), image_size));
                return b;
            };

            // crop out the shape from the input image
            auto bds = add_padding(Bounds(
                min_x, min_y,
                max_x - min_x + 1, max_y - min_y + 1
            ), 5, Size2(input.cols, input.rows));
            
            // initialize cv::Mat l
            input(bds).copyTo(l);
            
            // find the longest side of the shape
            Vec2 prev{ (float)c.back()[0], (float)c.back()[1] };
            size_t max_side = 0;
            float maximum = 0;
            for (size_t j = 0; j < c.size(); ++j) {
                Vec2 pos{ (float)c[j][0], (float)c[j][1] };
                float L = sqdistance(prev, pos);

                if (L > maximum) {
                    maximum = L;
                    max_side = j;
                }

                prev = pos;
            }

            // rotate the image according to its longest side
            auto rotate_image = [&_average](cv::Mat& l, cv::Mat& rotated, Bounds bds, const Vec2& p0, const Vec2& p1) -> pv::BlobPtr
            {
                auto angle = atan2((p1 - p0).normalize());
                auto rot = cv::getRotationMatrix2D(Size2(l) * 0.5, DEGREE(angle), 1.0);
                cv::warpAffine(l, rotated, rot, Size2(l));

                int cut_off = 4;
                const Size2 tag_size = tags_maximum_image_size;

                if (rotated.cols - cut_off * 2 > tag_size.width) {
                    cut_off = (rotated.cols - tag_size.width) / 2 + 1;
                }
                if (rotated.rows - cut_off * 2 > tag_size.height) {
                    cut_off = (rotated.rows - tag_size.height) / 2 + 1;
                }

                auto cut = Bounds(Vec2(cut_off), Size2(rotated) - cut_off * 2);
                cut.restrict_to(Bounds(Vec2(), Size2(rotated)));

                if (cut.height <= 0 || cut.width <= 0)
                    return nullptr;

                bds.x += cut_off;
                bds.y += cut_off;
                bds.width -= cut_off * 2;
                bds.height -= cut_off * 2;

                // force resize all tags to 32, 32. this is currently
                // TODO: make this a variable
                cv::resize(rotated(cut), l, Size2(32, 32), 0, 0, cv::INTER_LINEAR);
                
                bds.width = bds.height = l.cols;
                if (bds.x + bds.width > _average->cols)
                    bds.x = _average->cols - bds.width;
                if (bds.y + bds.height > _average->rows)
                    bds.y = _average->rows - bds.height;
                if (bds.y < 0)
                    bds.y = 0;
                if (bds.x < 0)
                    bds.x = 0;
                
                // convert to pixels and lines, pre-resize the array though
                auto lines = std::make_unique<cmn::blob::line_ptr_t::element_type>(l.rows);
                auto pixels = std::make_unique<cmn::blob::pixel_ptr_t::element_type>(size_t(l.rows) * size_t(l.cols) * size_t(l.channels()));

                std::copy(l.ptr(), l.ptr() + pixels->size(), pixels->begin());
    #ifndef NDEBUG
                if (bds.width != l.cols)
                    throw U_EXCEPTION("width ", bds, " != ", Size2(l));
    #endif

                for (int y = 0; y < l.rows; ++y) {
                    (*lines)[y].y = y + bds.y;
                    (*lines)[y].x0 = bds.x;
                    (*lines)[y].x1 = bds.x + bds.width - 1;
                }
                
                // return a blob
                return std::make_unique<pv::Blob>(
                        std::move(lines),
                        std::move(pixels),
                        pv::Blob::flag(pv::Blob::Flags::is_tag)
                            | pv::Blob::flag(pv::Blob::Flags::is_instance_segmentation)
                            | (l.channels() == 3
                                ? pv::Blob::flag(pv::Blob::Flags::is_rgb) : 0),
                        blob::Prediction{});
            };
            
            auto idx = (max_side == 0 ? c.size() : max_side) - 1;
            auto blob = rotate_image(l, rotated, bds,
                         Vec2{ (float)c[idx][0], (float)c[idx][1] },
                         Vec2{ (float)c[max_side][0], (float)c[max_side][1] });
            if(!blob)
                continue;
            
            blobs.emplace_back(std::move(blob));
        }
        else if (show_debug_info) {
            cv::drawContours(result, cache->contours, index, Color(150, 0, 0, 255));
            cv::putText(result, cmn::format<FormatterType::NONE>("area: ", area), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Red);
        }
    }

    std::unique_lock guard(mutex);
    cache->tags.insert(cache->tags.end(), std::make_move_iterator(blobs.begin()), std::make_move_iterator(blobs.end()));
};

RawProcessing::RawProcessing(const gpuMat &average, const gpuMat *float_average, const LuminanceGrid* ) 
    : _average(&average), _float_average(float_average)
{ }

void RawProcessing::generate_binary(const cv::Mat& /*cpu_input*/, const gpuMat& input, cv::Mat& output, TagCache* tag_cache) {
    assert(input.type() == CV_8UC1 || input.type() == CV_8UC3);

    static bool enable_diff = SETTING(enable_difference);
    static bool enable_abs_diff = SETTING(enable_absolute_difference);
    static bool blur_difference = SETTING(blur_difference);
    static std::once_flag registered_callback;
    static float adaptive_threshold_scale = 0;
    static int detect_threshold = 25, threshold_maximum = 255;
    static bool use_closing = false;
    static int closing_size = 1;
    static bool use_adaptive_threshold = false;
    static int32_t dilation_size = 0;
    static bool tags_enable = false, image_invert = false;
    
    std::call_once(registered_callback, [](){
        static const auto callback = [](std::string_view key) {
            auto& value = GlobalSettings::map().at(key).get();
            if (key == "enable_difference")
                enable_diff = value.template value<bool>();
            else if (key == "enable_absolute_difference")
                enable_abs_diff = value.template value<bool>();
            else if (key == "adaptive_threshold_scale")
                adaptive_threshold_scale = value.template value<float>();
            else if (key == "detect_threshold")
                detect_threshold = value.template value<int>();
            else if (key == "threshold_maximum")
                threshold_maximum = value.template value<int>();
            else if (key == "use_closing")
                use_closing = value.template value<bool>();
            else if (key == "closing_size")
                closing_size = value.template value<int>();
            else if (key == "use_adaptive_threshold")
                use_adaptive_threshold = value.template value<bool>();
            else if (key == "dilation_size")
                dilation_size = value.template value<int32_t>();
            else if (key == "image_invert")
                image_invert = value.template value<bool>();
            else if (key == "tags_enable")
                tags_enable = value.template value<bool>();
        };
        
        GlobalSettings::map().register_callbacks({
            "enable_difference",
            "enable_absolute_difference",
            "adaptive_threshold_scale",
            "detect_threshold",
            "threshold_maximum",
            "use_closing",
            "closing_size",
            "use_adaptive_threshold",
            "dilation_size",
            "image_invert",
            "tags_enable"
            
        }, callback);
    });

    // These two buffers are constantly going to be exchanged
    // after every call to a cv function. This means that no additional
    // resources need to be allocated (i.e. creating temporary buffers).
    gpuMat* INPUT, * OUTPUT;
    INPUT = &_buffer0;
    OUTPUT = &_buffer1;

    // These macros exchange them:
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
/*#ifndef NDEBUG
    #define CALLCV( X ) { X; std::swap(INPUT, OUTPUT); Print("(",__FILENAME__,":",__LINE__,") ",#X,": ",INPUT->cols,"x",INPUT->rows," vs. ",OUTPUT->cols,"x",OUTPUT->rows); } void()
#else*/
    #define CALLCV( X ) { X; std::swap(INPUT, OUTPUT); } void()
//#endif

    assert(_average->type() == CV_8UC(input.channels()));
    //assert(_buffer0.empty() || _buffer0.type() == CV_8UC(input.channels()));
    
    if (_average->empty()) {
        throw U_EXCEPTION("Average image is empty.");
    }

    // DO we need to invert the image? Cause if yes, then the average
    // would be inverted already (so we need to do the same with the
    // input).
    // Also, copy to GPU in the same step.
    if(input.channels() == 3) {
        assert(_average->channels() == 3);
        if(_grey_average.empty())
            cv::cvtColor(*_average, _grey_average, cv::COLOR_BGR2GRAY);
        
        cv::cvtColor(input, *INPUT, cv::COLOR_BGR2GRAY);
        if(image_invert)
            CALLCV(cv::subtract(255.0, *INPUT, *OUTPUT));
        
    } else if (image_invert) {
        cv::subtract(255.0, input, *INPUT);
        
    } else {
        input.copyTo(*INPUT);
    }

    if(blur_difference) {
        if (enable_abs_diff) {
            CALLCV(cv::absdiff(*INPUT, *_average, *OUTPUT));
        }
        else {
            CALLCV(cv::subtract(*_average, *INPUT, *OUTPUT));
        }
        
        if(tags_enable)
            INPUT->copyTo(_floatb0);
        
        CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_TOZERO));
        
        CALLCV(cv::blur(*INPUT, *OUTPUT, cv::Size(25, 25)));
        
        CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_BINARY));
        
    } else {
        
        if (enable_diff) {
            //tf::imshow("INPUT", *INPUT);
            
            if(_average->channels() == 1) {
                if (enable_abs_diff) {
                    CALLCV(cv::absdiff(*INPUT, *_average, *OUTPUT));
                }
                else {
                    CALLCV(cv::subtract(*_average, *INPUT, *OUTPUT));
                }
            } else {
                if (enable_abs_diff) {
                    CALLCV(cv::absdiff(*INPUT, _grey_average, *OUTPUT));
                }
                else {
                    CALLCV(cv::subtract(_grey_average, *INPUT, *OUTPUT));
                }
            }
            
            //if(not _grey_average.empty())
            //    tf::imshow("grey_average", _grey_average);
            //tf::imshow("subtracted", *INPUT);
            
            if(tags_enable)
                INPUT->copyTo(_floatb0);
        }
        
        if (dilation_size != 0) {
            static std::once_flag flag;
            std::call_once(flag, []() {
                const cv::Mat element = cv::Mat::ones(abs(dilation_size), abs(dilation_size), CV_8UC1);
                element.copyTo(gpu_dilation_element);
            });
            
            INPUT->copyTo(diff);
        }
        
        // calculate adaptive threshold neighborhood size
        int adaptive_neighborhood_size = INPUT->cols * adaptive_threshold_scale;
        if (adaptive_neighborhood_size % 2 == 0) {
            adaptive_neighborhood_size++;
        }
        if (adaptive_neighborhood_size < 3)
            adaptive_neighborhood_size = 3;
        
        // BIT: use_closing, will change the flow a bit.
        //      1. threshold
        //      2. dilate + erode
        //      3. use dilation flag
        if (use_closing) {
            static gpuMat closing_element;
            static std::once_flag flag;
            
            std::call_once(flag, []() {
                const int morph_size = closing_size;
                const cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * morph_size + 1, 2 * morph_size + 1), cv::Point(morph_size, morph_size));
                element.copyTo(closing_element);
            });
            
            if (use_adaptive_threshold) {
                //cv::Mat local;
                //INPUT->copyTo(local);
                //tf::imshow("INPUT", local);
                //CALLCV(cv::GaussianBlur(*INPUT, *OUTPUT, Size2(21,21), 0));
                //CALLCV(cv::threshold(*INPUT, *OUTPUT, threshold, 255, cv::THRESH_BINARY | cv::THRESH_OTSU));
                //cv::imwrite("/Users/tristan/output.png", local);
                
                /*cv::Mat local;
                 std::map<int, float> values;
                 float _ma = 0;
                 for(int threshold = 0; threshold < 100; threshold += 2) {
                 cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 23, -threshold);
                 OUTPUT->copyTo(local);
                 auto blobs = CPULabeling::run(local);
                 values[threshold] = blobs.size();
                 if(blobs.size() > _ma)
                 _ma = blobs.size();
                 }
                 
                 static cv::Mat bg = cv::Mat::zeros(480, 640, CV_8UC3);
                 static gui::CVBase base(bg);
                 static gui::DrawStructure s;
                 static gui::Graph g(Bounds(0, 0, 640,480), "Graph");
                 g.clear();
                 g.add_function(gui::Graph::Function("detect_threshold", gui::Graph::Type::DISCRETE, [values](float x) -> float {
                 if(!values.contains(int(x)))
                 return GlobalSettings::invalid();
                 return values.at(int(x));
                 }));
                 g.set_zero(0);
                 g.set_ranges(Rangef(0, 100), Rangef(0,_ma));
                 s.wrap_object(g);
                 base.paint(s);
                 base.display();*/
                
                //CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 25, -threshold));
                CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, adaptive_neighborhood_size, -detect_threshold));
                
                //INPUT->copyTo(local);
                //tf::imshow("OUTPUT "+Meta::toStr(adaptive_neighborhood_size), local);
            }
            else {
                if (threshold_maximum < 255) {
                    CALLCV(cv::inRange(*INPUT, detect_threshold, threshold_maximum, *OUTPUT));
                }
                else
                    CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_BINARY));
            }
            
            if (detect_threshold < 0) {
                CALLCV(cv::subtract(255, *INPUT, *OUTPUT));
            }
            
            CALLCV(cv::dilate(*INPUT, *OUTPUT, closing_element));
            CALLCV(cv::erode(*INPUT, *OUTPUT, closing_element));
            
            if (dilation_size > 0) {
                CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_dilation_element));
            }
            else if (dilation_size < 0) {
                CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_dilation_element));
                
                CALLCV(INPUT->convertTo(*OUTPUT, CV_8UC1));
                CALLCV(diff.copyTo(*OUTPUT, *INPUT));
                CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_BINARY));
                
                CALLCV(cv::dilate(*INPUT, *OUTPUT, closing_element));
                CALLCV(cv::erode(*INPUT, *OUTPUT, closing_element));
            }
        }
        else {
            // BIT: use_closing is FALSE, so:
            //      1. threshold
            //      2. check dilation_size flag
            if (use_adaptive_threshold) {
                CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, adaptive_neighborhood_size, -detect_threshold));
            }
            else {
                if (threshold_maximum < 255) {
                    CALLCV(cv::inRange(*INPUT, detect_threshold, threshold_maximum, *OUTPUT));
                }
                else {
                    CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_BINARY));
                }
            }
            
            if (detect_threshold < 0) {
                CALLCV(cv::subtract(255, *INPUT, *OUTPUT));
            }
            
            if (dilation_size > 0) {
                CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_dilation_element));
            }
            else if (dilation_size < 0) {
                CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_dilation_element));
                
                CALLCV(INPUT->convertTo(*OUTPUT, CV_8UC1));
                CALLCV(diff.copyTo(*OUTPUT, *INPUT));
                CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(detect_threshold), 255, cv::THRESH_BINARY));
            }
        }
    }

    // use the generated thresholded image as a mask
    // for the original input (so we can later retrieve
    // greyscale), and copy to output.
    if(input.channels() == 3) {
        /*gpuMat mask;
        std::vector<gpuMat> channels;
        cv::split(*OUTPUT, channels);
        
        
        cv::Mat cpu;
        channels.front().copyTo(cpu);
        tf::imshow("B", cpu);
        
        channels.at(1).copyTo(cpu);
        tf::imshow("G", cpu);
        
        channels.back().copyTo(cpu);
        tf::imshow("R", cpu);
        
        cv::bitwise_or(channels.front(), channels.back(), mask);
        cv::bitwise_or(mask, channels.at(1), mask);
        
        mask.copyTo(cpu);
        tf::imshow("mask", cpu);
        input.copyTo(output, mask);*/
        //tf::imshow("mask", *INPUT);
        
        std::vector<gpuMat> channels;
        cv::split(input, channels);
        
        gpuMat B, G, R;
        cv::bitwise_and(*INPUT, channels.front(), B);
        cv::bitwise_and(*INPUT, channels.at(1), G);
        cv::bitwise_and(*INPUT, channels.back(), R);
        
        //tf::imshow("B", B);
        //tf::imshow("G", G);
        //tf::imshow("R", R);
        
        cv::merge(std::vector<gpuMat>{B, G, R}, output);
        //OUTPUT->copyTo(output);
        //input.copyTo(output, *INPUT);
        
    } else {
        cv::bitwise_and(*INPUT, input, *OUTPUT);
        OUTPUT->copyTo(output);
    }
        
    //tf::imshow("input", input);
    //tf::imshow("output", output);
    
    // BIT: tags_enable
    // This will only be traversed if we need to recognize tags
    // as well. Tags are rectangles in the image that contain
    // some kind of pattern. Typically, these would be squares
    // with a black outline, white background and black patterns
    // inside.
    if (tags_enable) {
        INPUT = &_floatb0;
        OUTPUT = &_buffer1;
        
        static const bool show_debug_info = SETTING(tags_debug).value<bool>();
        cv::Mat local, result;
        
        /*if(show_debug_info) {
            INPUT->copyTo(local);
            Print(getImgType(local.type()));
            tf::imshow("only_bg_invert", local);
        }*/

        if (!enable_diff) {
            if (OUTPUT->cols != input.cols || OUTPUT->rows != input.rows || OUTPUT->type() != input.type())
                FormatWarning("OUTPUT != input: ", Size2(input.cols, input.rows), " OUTPUT: ", Size2(OUTPUT->cols, OUTPUT->rows));
            CALLCV(cv::subtract(*_average, input, *OUTPUT));
        }

        static const auto tags_equalize_hist = SETTING(tags_equalize_hist).value<bool>();
        static const auto tags_threshold = SETTING(tags_threshold).value<int>();
        
        // BIT: tags_equalize_hist
        // Will stretch the histogram of the tag difference image so that
        // the range goes from 0-255.
        if (tags_equalize_hist)
            CALLCV(cv::equalizeHist(*INPUT, *OUTPUT));
        
        // blur everything a bit to merge some of the outlines together
        CALLCV(cv::blur(*INPUT, *OUTPUT, cv::Size(3, 3)));
        
        // apply adaptive threshold to make results more robust
        CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 15, tags_threshold));
        
        // invert
        CALLCV(cv::subtract(255, *INPUT, *OUTPUT));

        tag_cache->tags.clear();
        tag_cache->contours.clear();
        tag_cache->hierarchy.clear();
        
        // use the find contours algorithm to retrieve a hierarchy of lines
        // in the image. we will then be searching for anything that does
        // not have a parent, but does have something inside, AND is square
        // shape (of a certain size).
        cv::findContours(*INPUT, tag_cache->contours, tag_cache->hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
        
        // debug info can be generated with the tags_debug bit.
        if (show_debug_info)
            cv::cvtColor(input, result, cv::COLOR_GRAY2BGRA);
        
        gpuMat inverted_input;
        if(image_invert)
            cv::subtract(255, input, inverted_input);
        else
            input.copyTo(inverted_input);
        
        if (show_debug_info) {
            inverted_input.copyTo(local);
            //tf::imshow("inverted", local);
        }

        // calculate in multiple threads (the contours array)
        distribute_indexes([&](auto index, auto start, auto end, auto){
            process_tags(narrow_cast<int>(index), start, end, result, tag_cache, INPUT, inverted_input, _average);
        }, _contour_pool, tag_cache->contours.begin(), tag_cache->contours.end());

        if (show_debug_info) {
            //resize_image(result, 0.5, cv::INTER_LINEAR);
            cv::cvtColor(result, result, cv::COLOR_BGRA2RGBA);
            //tf::imshow("result", result);
        }
    }
}

}
