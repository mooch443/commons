#include "RawProcessing.h"
#include "CPULabeling.h"
#include "misc/GlobalSettings.h"
#include "misc/Timer.h"
#include <misc/ocl.h>
#include <processing/LuminanceGrid.h>
#include <misc/ranges.h>
#include <processing/PadImage.h>
#include <misc/ThreadPool.h>

#define REC_TAGS
#define DEBUG_TAGS
using namespace cmn;

gpuMat gpu_dilation_element;
std::shared_mutex mutex;

GenericThreadPool _contour_pool(5);

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

void process_tags(auto index,
                  auto start,
                  auto end,
                  const auto& result,
                  const auto& hierarchy,
                  const auto& contours,
                  const auto* INPUT,
                  const auto& input,
                  const auto& _average,
                  auto& tags)
{
    using namespace gui;
    
    static const double cm_per_pixel = SETTING(cm_per_pixel).value<float>() <= 0 ? 234.0 / 3007.0 : SETTING(cm_per_pixel).value<float>();
    static const Range<double> tag_area_range = SETTING(tags_size_range).value<Range<double>>();
    
    static const auto tags_equalize_hist = SETTING(tags_equalize_hist).value<bool>();
    static const auto tags_threshold = SETTING(tags_threshold).value<uchar>();
    static const auto tags_num_sides = SETTING(tags_num_sides).value<Range<int>>();
    static const auto tags_approximation = SETTING(tags_approximation).value<float>();
#ifdef DEBUG_TAGS
        static const bool show_debug_info = SETTING(tags_debug).value<bool>();
        cv::Mat local;
        if (show_debug_info) {
            INPUT->copyTo(local);
            //INPUT->convertTo(local, CV_8UC1, 255.0);
            tf::imshow("only_bg", local);
        }
#endif
    cv::Mat l;
    cv::Mat rotated;
    cv::Mat smaller;
    std::vector<pv::BlobPtr> blobs;

    for (auto it = start; it != end; ++it, ++index) {
        auto& c = *it;
        auto& h = hierarchy[index];

        if (c.size() < 2 || (h[2] == -1 && h[3] != -1)) {
#ifdef DEBUG_TAGS
            if (show_debug_info)
                cv::drawContours(result, contours, index, Color(50, 0, 0, 255));
#endif
            continue;
        }

        auto perimeter = cv::arcLength(c, true);
        cv::approxPolyDP(cv::Mat(c), c, tags_approximation * perimeter, true);
        if (!tags_num_sides.contains(c.size())) {
#ifdef DEBUG_TAGS
            if (show_debug_info) {
                cv::drawContours(result, contours, index, Color(50, 50, 0, 255));

                int min_x = INPUT->cols, min_y = INPUT->rows,
                    max_x = 0, max_y = 0;

                for (int j = 0; j < c.size(); ++j) {
                    auto& pt = c[j];
                    min_x = min(pt[0], min_x);
                    max_x = max(pt[0], max_x);
                    min_y = min(pt[1], min_y);
                    max_y = max(pt[1], max_y);
                }
                cv::putText(result, cmn::format<FormatterType::NONE>("sides: ", c.size()), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Red);
            }
#endif
            continue;
        }

        auto area = cv::contourArea(c, false);
        if (area == 0) {
            continue;
        }

        area = sqrt(area * SQR(cm_per_pixel));

        int min_x = INPUT->cols, min_y = INPUT->rows,
            max_x = 0, max_y = 0;

        for (int j = 0; j < c.size(); ++j) {
            auto& pt = c[j];
            min_x = min(pt[0], min_x);
            max_x = max(pt[0], max_x);
            min_y = min(pt[1], min_y);
            max_y = max(pt[1], max_y);
        }

        if (tag_area_range.contains(area)) {
            //++found;
#ifdef DEBUG_TAGS
            //auto N = (max_y - min_y + 1) * (max_x - min_x + 1);
            //auto b = input(Bounds(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1));

           // auto p = cv::sum(b)[0] / 255.0 / double(N);

            //print("area: ", area, " white/black ratio: ", p, " N: ", N, " sides: ", c.size());

            //if (Range<double>(0.84, 0.95).contains(p))
            //{
            if (show_debug_info) {
                cv::line(result, Vec2(min_x, min_y), Vec2(max_x, min_y), Cyan, 2);
                cv::line(result, Vec2(max_x, min_y), Vec2(max_x, max_y), Cyan, 2);
                cv::line(result, Vec2(max_x, max_y), Vec2(min_x, max_y), Cyan, 2);
                cv::line(result, Vec2(min_x, max_y), Vec2(min_x, min_y), Cyan, 2);

                cv::putText(result, cmn::format<FormatterType::NONE>("area: ", area, " sides: ", c.size()), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Cyan);
                //}
               //else {
                cv::drawContours(result, contours, index, Color(255, 150, 0, 255));
            }
#endif
            auto add_padding = [](Bounds bds, float padding, const Size2& image_size) -> Bounds {
                Bounds b(bds.x - padding, bds.y - padding, bds.width + padding * 2, bds.height + padding * 2);
                b.restrict_to(Bounds(Vec2(0), image_size));
                return b;
            };

            auto bds = add_padding(Bounds(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1), 5, Size2(input.cols, input.rows));
            input(bds).copyTo(l);
            //cv::equalizeHist(l, l);
           //.copyTo(l);
            //resize_image(l, l, 5, cv::INTER_LINEAR);
            //tf::imshow("tag", l);

            Vec2 prev{ (float)c.back()[0], (float)c.back()[1] };
            int max_side = 0;
            float maximum = 0;
            for (int j = 0; j < c.size(); ++j) {
                Vec2 pos{ (float)c[j][0], (float)c[j][1] };
                float L = sqdistance(prev, pos);

                if (L > maximum) {
                    maximum = L;
                    max_side = j;
                }

                prev = pos;
            }

            //print("max side: ", max_side, " max length: ", maximum);
            {
                auto idx = (max_side == 0 ? c.size() : max_side) - 1;
                //print("max_side = ", max_side, " idx = ", idx, " L = ", c.size());
                Vec2 prev = Vec2{ (float)c[idx][0], (float)c[idx][1] };//.normalize();
                Vec2 pos = Vec2{ (float)c[max_side][0], (float)c[max_side][1] };//.normalize();

               // auto angle = atan2(prev.x* pos.y - pos.x * prev.y, prev.x* pos.x + prev.y * pos.y);
                //print("pos:", pos, " prev:", prev);
                //auto angle = atan2(pos) - atan2(prev);
                auto angle = atan2((pos - prev).normalize());
                //if (angle > M_PI) { angle -= 2 * M_PI; }
                //else if (angle <= -M_PI) { angle += 2 * M_PI; }

                //print("m = ", angle);

                /*float max_height;
                auto max_width = euclidean_distance(prev, pos);

                {
                    max_side = (max_side + 1) % 4;
                    idx = (max_side == 0 ? c.size() : max_side) - 1;
                    Vec2 prev0 = Vec2{ (float)c[idx][0], (float)c[idx][1] };//.normalize();
                    Vec2 pos0 = Vec2{ (float)c[max_side][0], (float)c[max_side][1] };//.normalize();

                    max_side = (max_side + 2) % 4;

                    idx = (max_side == 0 ? c.size() : max_side) - 1;
                    Vec2 prev1 = Vec2{ (float)c[idx][0], (float)c[idx][1] };//.normalize();
                    Vec2 pos1 = Vec2{ (float)c[max_side][0], (float)c[max_side][1] };//.normalize();

                    if (sqdistance(prev0, pos0) > sqdistance(prev1, pos1)) {
                        max_height = euclidean_distance(prev0, pos0);
                    }
                    else {
                        max_height = euclidean_distance(prev1, pos1);
                    }

                    auto L1 = [](auto& A, auto& B) {
                        return sqrt(SQR((float)A[0] - (float)B[0]) + SQR((float)A[1] - (float)B[1]));
                    };
                    auto widthAB = L1(c[0], c[1]);
                    auto widthBC = L1(c[1], c[2]);
                    auto widthCD = L1(c[2], c[3]);
                    auto widthAD = L1(c.front(), c.back());
                    auto maxWidth = max((widthAD), (widthBC));
                    auto maxHeight = max((widthAB), (widthCD));


                    std::vector<cv::Point2f> output_pts{
                        {0,0}, {(float)maxWidth, 0}, {(float)maxWidth,(float)maxHeight}, {0, (float)maxHeight}
                    };

                    cv::Mat input_pts;
                    cv::Mat(c).convertTo(input_pts, CV_32FC2);
                    auto transform = cv::getPerspectiveTransform(input_pts, output_pts);
                    cv::warpPerspective(l, rotated, transform, Size2(l.cols, l.rows), cv::INTER_LINEAR);
                    if (rotated.type() != CV_8UC1) {
                        print("type != CV_8UC1");
                    }
                }*/

                auto rot = cv::getRotationMatrix2D(Size2(l) * 0.5, DEGREE(angle), 1.0);
                cv::warpAffine(l, rotated, rot, Size2(l));

                int cut_off = 4;
                auto xy = Size2(rotated) * 0.5 - (Size2(rotated) * 0.5 - cut_off);
                const Size2 tag_size(80, 80);

                if (rotated.cols - cut_off * 2 > tag_size.width) {
                    cut_off = (rotated.cols - tag_size.width) / 2 + 1;
                }
                if (rotated.rows - cut_off * 2 > tag_size.height) {
                    cut_off = (rotated.rows - tag_size.height) / 2 + 1;
                }

                auto cut = Bounds(Vec2(cut_off), Size2(rotated) - cut_off * 2);
                cut.restrict_to(Bounds(Vec2(), Size2(rotated)));

                if (cut.height <= 0 || cut.width <= 0)
                    continue;

                //
#ifdef DEBUG_TAGS
                if (show_debug_info) {
                    cv::cvtColor(l, smaller, cv::COLOR_GRAY2BGR);
                    resize_image(smaller, 5, cv::INTER_LINEAR);

                    prev = Vec2(c.back()[0], c.back()[1]);
                    for (auto j = 0; j < c.size(); ++j) {
                        Vec2 pos(c[j][0], c[j][1]);
                        cv::line(smaller, (prev - bds.pos() + 0.5) * 5, (pos - bds.pos() + 0.5) * 5, Cyan);
                    }

                    tf::imshow("original", smaller);
                }
#endif
                //

                bds.x += cut_off;
                bds.y += cut_off;
                bds.width -= cut_off * 2;
                bds.height -= cut_off * 2;

                //cut.restrict_to(Bounds(Vec2(0), Size2(32, 32)));

                rotated(cut).copyTo(smaller);

                cv::resize(smaller, l, Size2(32, 32), 0, 0, cv::INTER_LINEAR);
                bds.width = bds.height = l.cols;
                if (bds.x + bds.width > _average->cols)
                    bds.x = _average->cols - bds.width;
                if (bds.y + bds.height > _average->rows)
                    bds.y = _average->rows - bds.height;
                if (bds.y < 0)
                    bds.y = 0;
                if (bds.x < 0)
                    bds.x = 0;
                //pad_image(l, smaller, tag_size,-1, true, cv::Mat(),255);
                //smaller.copyTo(l);

                //
#ifdef DEBUG_TAGS
                if (show_debug_info) {
                    resize_image(smaller, 5, cv::INTER_LINEAR);
                    tf::imshow("rotated", smaller);
                }
#endif
            }

            auto lines = std::make_unique<cmn::blob::line_ptr_t::element_type>();
            auto pixels = std::make_unique<cmn::blob::pixel_ptr_t::element_type>();
            pixels->resize(size_t(l.rows) * size_t(l.cols));
            lines->resize(l.rows);

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

            blobs.emplace_back(std::make_unique<pv::Blob>(std::move(lines), std::move(pixels)));
            //cv::imwrite("C:/Users/tristan/tag_images/" + tags.back()->blob_id().toStr() + ".png", l);
#ifdef DEBUG_TAGS
        /*auto&& [pos, img] = blobs.back()->image(nullptr, Bounds(-1, -1, -1, -1), 0);
        cv::cvtColor(img->get(), l, cv::COLOR_GRAY2BGR);
        if (l.cols != 32 || l.rows != 32) {
            print("Size different: ", Size2(l));
        }


        resize_image(l, 5, cv::INTER_LINEAR);
        tf::imshow("tag", l);*/
#endif
        // }

        }
        else {
#ifdef DEBUG_TAGS
            if (show_debug_info) {
                cv::drawContours(result, contours, index, Color(150, 0, 0, 255));
                cv::putText(result, cmn::format<FormatterType::NONE>("area: ", area), Vec2(min_x, min_y - 10), cv::FONT_HERSHEY_PLAIN, 0.6, Red);
            }
#endif
        }
    }

    std::unique_lock guard(mutex);
    tags.insert(tags.end(), std::make_move_iterator(blobs.begin()), std::make_move_iterator(blobs.end()));
};

RawProcessing::RawProcessing(const gpuMat &average, const gpuMat *float_average, const LuminanceGrid* ) 
    : _average(&average), _float_average(float_average)
{ }

void RawProcessing::generate_binary(const cv::Mat& cpu_input, const gpuMat& input, cv::Mat& output, std::vector<pv::BlobPtr>& tags) {
    assert(input.type() == CV_8UC1);

    static bool enable_diff = SETTING(enable_difference);
    static bool enable_abs_diff = SETTING(enable_absolute_difference);
    static std::atomic_bool registered_callback = false;
    static float adaptive_threshold_scale = 0;
    static int threshold = 25, threshold_maximum = 255;
    static bool use_closing = false;
    static int closing_size = 1;
    static bool use_adaptive_threshold = false;
    static int32_t dilation_size = 0;
    static bool image_adjust = false, image_square_brightness = false, image_invert = false, tags_enable = false;
    static float image_contrast_increase = 1, image_brightness_increase = 0;

    bool expected = false;
    if (registered_callback.compare_exchange_strong(expected, true)) {
        const char* ptr = "RawProcessing";
        sprite::Map::callback_func callback = [ptr](sprite::Map::Signal signal, sprite::Map& map, auto& key, auto& value)
        {
            if (signal == sprite::Map::Signal::EXIT) {
                map.unregister_callback(ptr);
                return;
            }

            if (key == std::string("enable_difference"))
                enable_diff = value.template value<bool>();
            else if (key == std::string("enable_absolute_difference"))
                enable_abs_diff = value.template value<bool>();
            else if (key == std::string("adaptive_threshold_scale"))
                adaptive_threshold_scale = value.template value<float>();
            else if (key == std::string("threshold"))
                threshold = value.template value<int>();
            else if (key == std::string("threshold_maximum"))
                threshold_maximum = value.template value<int>();
            else if (key == std::string("use_closing"))
                use_closing = value.template value<bool>();
            else if (key == std::string("closing_size"))
                closing_size = value.template value<int>();
            else if (key == std::string("use_adaptive_threshold"))
                use_adaptive_threshold = value.template value<bool>();
            else if (key == std::string("dilation_size"))
                dilation_size = value.template value<int32_t>();
            else if (key == std::string("image_adjust"))
                image_adjust = value.template value<bool>();
            else if (key == std::string("image_square_brightness"))
                image_square_brightness = value.template value<bool>();
            else if (key == std::string("image_contrast_increase"))
                image_contrast_increase = value.template value<float>();
            else if (key == std::string("image_brightness_increase"))
                image_brightness_increase = value.template value<float>();
            else if (key == std::string("image_invert"))
                image_invert = value.template value<bool>();
#if defined(REC_TAGS)
            else if (key == std::string("tags_enable"))
                tags_enable = value.template value<bool>();
#endif
        };
        GlobalSettings::map().register_callback(ptr, callback);

        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "enable_difference", GlobalSettings::get("enable_difference").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "enable_absolute_difference", GlobalSettings::get("enable_absolute_difference").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "square_brightness", GlobalSettings::get("square_brightness").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "adaptive_threshold_scale", GlobalSettings::get("adaptive_threshold_scale").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "threshold", GlobalSettings::get("threshold").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "threshold_maximum", GlobalSettings::get("threshold_maximum").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "use_closing", GlobalSettings::get("use_closing").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "closing_size", GlobalSettings::get("closing_size").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "use_adaptive_threshold", GlobalSettings::get("use_adaptive_threshold").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "dilation_size", GlobalSettings::get("dilation_size").get());

        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "image_adjust", GlobalSettings::get("image_adjust").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "image_square_brightness", GlobalSettings::get("image_square_brightness").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "image_contrast_increase", GlobalSettings::get("image_contrast_increase").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "image_brightness_increase", GlobalSettings::get("image_brightness_increase").get());
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "image_invert", GlobalSettings::get("image_invert").get());
#if defined(REC_TAGS)
        callback(sprite::Map::Signal::NONE, GlobalSettings::map(), "tags_enable", GlobalSettings::get("tags_enable").get());
#endif
    }

    //static Timing timing("thresholding", 30);
    //TakeTiming take(timing);

    gpuMat* INPUT, * OUTPUT;
    INPUT = &_buffer0;
    OUTPUT = &_buffer1;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#ifndef NDEBUG
#define CALLCV( X ) { X; std::swap(INPUT, OUTPUT); print("(",__FILENAME__,":",__LINE__,") ",#X,": ",INPUT->cols,"x",INPUT->rows," vs. ",OUTPUT->cols,"x",OUTPUT->rows); }
#else
#define CALLCV( X ) { X; std::swap(INPUT, OUTPUT); }
#endif

    //cv::Mat after_threshold, after_dilation;

    if (_average->empty()) {
        throw U_EXCEPTION("Average image is empty.");
    }

    assert(_average->type() == CV_8UC1);
    assert(_buffer0.type() == CV_8UC1);

    //cv::equalizeHist( input, _buffer0 );
    //cv::equalizeHist(*_average, _buffer1);
    //cv::absdiff(_buffer0, _buffer1, _buffer1);

    /*cv::Mat m;
    input.copyTo(m);
    tf::imshow("input", m);
    _average->copyTo(m);
    tf::imshow("average", m);*/

#ifndef NDEBUG
    print("enable_diff ", enable_diff, " / enable_abs_diff ", enable_abs_diff);
    print("Average: ", _average->cols, "x", _average->rows, " Input: ", input.cols, "x", input.rows);
#endif

    /*cv::Mat local;
    input.copyTo(local);
    tf::imshow("->input", local);*/
    if (image_invert) {
        cv::subtract(cv::Scalar(255), input, *INPUT);
    }
    else
        input.copyTo(*INPUT);

    //INPUT->copyTo(local);
    //tf::imshow("->input->", local);

    if (enable_diff) {
        if (enable_abs_diff) {
            CALLCV(cv::absdiff(*INPUT, *_average, *OUTPUT));
        }
        else {
            CALLCV(cv::subtract(*_average, *INPUT, *OUTPUT));
        }

        INPUT->copyTo(_floatb0);
        //INPUT->copyTo(local);
        //tf::imshow("difference", local);

    }

    /*if(image_adjust) {

        //cv::Mat local;
        //_buffer1.copyTo(local);
        //tf::imshow("before", local);

        float alpha = image_contrast_increase / 255.f;
        float beta = image_brightness_increase;
        _buffer1.convertTo(_buffer0, CV_32FC1, alpha, beta);

        if(image_square_brightness)
            cv::multiply(_buffer0, _buffer0, _buffer0);

        // normalize resulting values between 0 and 1
        cv::threshold(_buffer0, _buffer0, 1, 1, cv::THRESH_TRUNC);
        //cv::equalizeHist(_buffer1, _buffer0);

        //_buffer1.convertTo(_buffer0, CV_32FC1, 1./255.f);

        //cv::add(_buffer0, 1, _buffer1);
        //cv::multiply(_buffer1, _buffer1, _buffer0);
        //cv::multiply(_buffer0, _buffer0, _buffer1);


        //cv::multiply(_buffer1, _buffer1, _buffer1);
        //cv::subtract(_buffer1, 1, _buffer0);

        //cv::threshold(_buffer0, _buffer0, 1, 1, CV_THRESH_TRUNC);
        //cv::multiply(_buffer0, 255, _buffer0);

        _buffer0.convertTo(_buffer1, CV_8UC1, 255);
        //_buffer1.copyTo(local);
        //tf::imshow("after", local);

    } else if(_grid) {
        _buffer1.convertTo(_buffer0, CV_32FC1);
        cv::divide(_buffer0, _grid->relative_brightness(), _buffer1);
        _buffer1.convertTo(_buffer1, CV_8UC1);
    }*/

    int gauss = INPUT->cols * adaptive_threshold_scale;
    if (gauss % 2 == 0) {
        gauss++;
    }
    if (gauss < 3)
        gauss = 3;

    if (dilation_size) {
        std::shared_lock guard(mutex);
        if (gpu_dilation_element.empty()) {
            guard.unlock();

            std::unique_lock full_lock(mutex);
            const cv::Mat dilation_element = cv::Mat::ones(abs(dilation_size), abs(dilation_size), CV_8UC1); //cv::getStructuringElement( cv::MORPH_ELLIPSE, cv::Size( 2*abs(dilation_size) + 1, 2*abs(dilation_size)+1 ), cv::Point( abs(dilation_size), abs(dilation_size) ) );

#ifndef NDEBUG
            print("copy ", dilation_element.cols, ",", dilation_element.rows, " ", dilation_size, " ", gpu_dilation_element.cols, "x", gpu_dilation_element.rows);
#endif
            dilation_element.copyTo(gpu_dilation_element);
        }
    }

#ifndef NDEBUG
    print("dilation_size ", dilation_size);
#endif
    if (dilation_size != 0)
        INPUT->copyTo(diff);

    /*cv::Mat dist_transform;
    cv::Mat local, sure_bg, sure_fg, bgr;
    cv::Mat unknown;



    auto segment = [&]() {
        std::lock_guard<std::mutex> guard(mutex);
        cv::dilate(_buffer1, sure_bg, gpu_dilation_element, Vec2(-1), 3);


        cv::distanceTransform(_buffer1, dist_transform, labels, cv::DIST_L2, 5);

        double mi, ma;
        cv::minMaxLoc(dist_transform, &mi, &ma);
        cv::threshold(dist_transform, sure_fg, 0.65 * ma, 255, 0);
        sure_fg.convertTo(sure_fg, CV_8UC1);
        tf::imshow("sure_fg", sure_fg);

        dist_transform.convertTo(local, CV_8UC1, 20);
        tf::imshow("dist_transform", local);

        cv::subtract(sure_bg, sure_fg, unknown);

        cv::connectedComponents(sure_fg, labels);
        cv::add(labels, cv::Scalar(1), labels);

        labels.convertTo(local, CV_8UC1);
        resize_image(local, 0.3);
        tf::imshow("labels1", local);

        labels.setTo(0, unknown);

        cv::minMaxLoc(labels, &mi, &ma);
        labels.convertTo(local, CV_8UC1, 255.0 / ma);
        resize_image(local, 0.3);
        tf::imshow("labels2", local);

        tf::imshow("unknown", 255 - unknown);

        cv::cvtColor(_buffer1, bgr, cv::COLOR_GRAY2BGR);
        cv::watershed(bgr, labels);

        cv::Mat mask;
        cv::inRange(labels, -1, -1, mask);
        labels.setTo(0, mask);

        cv::minMaxLoc(labels, &mi, &ma);
        labels.convertTo(local, CV_8UC1, 255.0 / ma);
        resize_image(local, 0.3);
        tf::imshow("labels3", local);

        cv::minMaxLoc(labels, &mi, &ma);
        print(mi," ",ma);
        //cv::cvtColor(_buffer1, _buffer1, cv::COLOR_BGR2GRAY);
        cv::Mat labels2;
        cv::inRange(labels, 2, ma + 1, labels2);
        //cv::minMaxLoc(labels, &mi, &ma);

        cv::minMaxLoc(labels2, &mi, &ma);
        print("second ", mi," - ",ma," Empty");

        labels2.convertTo(_buffer1, CV_8UC1);
        labels2.copyTo(local);
        resize_image(local, 0.3);
        tf::imshow("labels4", local * 255.0 / ma);

        //cv::threshold(labels, _buffer1, 1, 255, cv::THRESH_BINARY);
    };*/

    if (use_closing) {
        if (use_adaptive_threshold) {
            CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, gauss, -threshold))
        }
        else {
            if (threshold_maximum < 255) {
                CALLCV(cv::inRange(*INPUT, threshold, threshold_maximum, *OUTPUT))
            }
            else
                CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(threshold), 255, cv::THRESH_BINARY))
        }

        if (threshold < 0) {
            CALLCV(cv::subtract(255, *INPUT, *OUTPUT))
        }

        static const int morph_size = abs(closing_size);
        static const cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * morph_size + 1, 2 * morph_size + 1), cv::Point(morph_size, morph_size));
        static gpuMat gpu_element;
        if (gpu_element.empty())
            element.copyTo(gpu_element);

        CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_element))
            CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_element))

            if (dilation_size > 0) {
                CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_dilation_element))
            }
            else if (dilation_size < 0) {
                CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_dilation_element))

                    CALLCV(INPUT->convertTo(*OUTPUT, CV_8UC1))
                    CALLCV(diff.copyTo(*OUTPUT, *INPUT))
                    CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(threshold), 255, cv::THRESH_BINARY))

                    CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_element))
                    CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_element))
            }

    }
    else {
        static const int morph_size = closing_size;
        static const cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * morph_size + 1, 2 * morph_size + 1), cv::Point(morph_size, morph_size));
        static gpuMat gpu_element;
        static std::once_flag flag;

        std::call_once(flag, []() {
            if (gpu_element.empty())
                element.copyTo(gpu_element);
        });

        if (use_adaptive_threshold) {
            CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, gauss, -threshold))

        }
        else {
            if (threshold_maximum < 255) {
                CALLCV(cv::inRange(*INPUT, threshold, threshold_maximum, *OUTPUT))
            }
            else {
                CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(threshold), 255, cv::THRESH_BINARY))
            }
        }

        if (threshold < 0) {
            CALLCV(cv::subtract(255, *INPUT, *OUTPUT))
        }

        //_buffer1.copyTo(after_threshold);

        if (dilation_size > 0) {
            CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_dilation_element))

        }
        else if (dilation_size < 0) {
            CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_dilation_element))

                CALLCV(INPUT->convertTo(*OUTPUT, CV_8UC1))
                CALLCV(diff.copyTo(*OUTPUT, *INPUT))
                CALLCV(cv::threshold(*INPUT, *OUTPUT, abs(threshold), 255, cv::THRESH_BINARY))

                /*cv::erode(_buffer1, _buffer1, gpu_dilation_element);
                cv::erode(_buffer1, _buffer1, gpu_dilation_element);
                //cv::morphologyEx(_buffer1, _buffer1, cv::MORPH_TOPHAT, gpu_dilation_element);

                _buffer1.convertTo(_buffer0, CV_8UC1);
                diff.copyTo(_buffer1, _buffer0);
                cv::threshold(_buffer1, _buffer1, abs(threshold), 255, cv::THRESH_BINARY);*/
                //segment();
        }

        //_buffer1.copyTo(after_dilation);
    }

    //cv::fastNlMeansDenoising(vec[1], vec[1]);
    //cv::adaptiveThreshold(_source, _binary, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY_INV, gauss, SETTING(threshold).value<int>());

    /*if(median_blur)
        _buffer1.copyTo(binary);
    else*/
    /*resize_image(after_dilation, 0.3);
    resize_image(after_threshold, 0.3);


    tf::imshow("after_dilation", after_dilation);
    tf::imshow("after_threshold", after_threshold);*/


    /*input.copyTo(normalized);
    cv::imwrite("C:/Users/tristan/test.png", normalized);
    cv::Mat tmp3, inverted;
    cv::threshold(normalized, tmp3, 150, 255, cv::THRESH_BINARY);
    //normalized.copyTo(tmp3, 255 - tmp3);
    //normalized = 255 - normalized;
    tmp3.copyTo(inverted);

    std::vector<cv::Mat> contours;
    //cv::equalizeHist(tmp3, inverted);

    //cv::Canny(tmp3, inverted, 250, 255);
    std::vector<cv::Vec4i> hierarchy;
    //cv::equalizeHist(normalized, normalized);
    cv::findContours(inverted, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    cv::Mat local;
    input.copyTo(local);
    //tf::imshow("input", local);
    //tf::imshow("normalized", normalized);
    //tf::imshow("inverted", inverted);

    cv::Mat opt;
    cv::Mat polygon;
    cv::cvtColor(inverted, opt, cv::COLOR_GRAY2BGRA);
    //opt = cv::Mat::zeros(output.rows, output.cols, CV_8UC4);
    //output.setTo(cv::Scalar(0));
    for(size_t i=0; i<contours.size(); ++i) {
        auto &c = contours[i];
        auto &h = h;

        //if(h[0] < 0 || h[3] >= 0)
        //    continue;

        auto perimeter = cv::arcLength(c, true);
        cv::approxPolyDP(c, polygon, 0.01f * perimeter, true);
        auto area = cv::contourArea(polygon, true);
        auto peri_area_ratio = (uint)abs(perimeter / area);

        using namespace gui;
        Color color = Color(150, 0, 0, 255);

        if(peri_area_ratio <= 1) {
            const float area_min = 5, area_max = 100;
            bool area_sign = true;
            bool quad_test = polygon.rows >= 4
                              //&& std::signbit(area) == area_sign
                              && Rangef(area_min, area_max).contains(abs(area))
                              && cv::isContourConvex(polygon);
            if(quad_test)
                color = Cyan;
        }

        if(c.dims != 2)
            throw U_EXCEPTION("E");
        Vec2 prev(-1);
        for (int i=0; i<c.rows; ++i) {
            auto pt = c.at<cv::Point2i>(i);
            if(prev.x > 0)
                cv::line(opt, prev, pt, color);
            prev = pt;
        }
    }

    cv::cvtColor(opt, opt, cv::COLOR_BGRA2RGBA);
    tf::imshow("opt", opt);*/

    //CALLCV(INPUT->convertTo(*OUTPUT, CV_8UC1))
    cv::bitwise_and(*INPUT, input, *OUTPUT);
    OUTPUT->copyTo(output);
        //CALLCV(cv::subtract(255, *INPUT, *OUTPUT));
    //output.setTo(cv::Scalar(0), *INPUT);
    //cpu_input.copyTo(output, *INPUT);
    //OUTPUT->copyTo(binary);

   /* cv::Mat __binary;
    binary.copyTo(__binary);
    resize_image(__binary, 0.3);
    tf::imshow("binary", __binary);*/
    /*cv::Mat local;
    _average->copyTo(local);
    tf::imshow("average", local);
    tf::imshow("output", output);
    INPUT->copyTo(local);
    tf::imshow("mask", local);*/
    //std::this_thread::sleep_for(std::chrono::milliseconds(100));

#ifdef REC_TAGS
#define DEBUG_TAGS
    if (tags_enable) {

        INPUT = &_floatb0;
        OUTPUT = &_buffer1;

        if (!enable_diff) {
            if (OUTPUT->cols != input.cols || OUTPUT->rows != input.rows || OUTPUT->type() != input.type())
                FormatWarning("OUTPUT != input: ", Size2(input.cols, input.rows), " OUTPUT: ", Size2(OUTPUT->cols, OUTPUT->rows));
            CALLCV(cv::subtract(*_average, input, *OUTPUT));
        }

        static const auto tags_equalize_hist = SETTING(tags_equalize_hist).value<bool>();
        static const auto tags_threshold = SETTING(tags_threshold).value<uchar>();
        static const auto tags_num_sides = SETTING(tags_num_sides).value<Range<int>>();
        static const auto tags_approximation = SETTING(tags_approximation).value<float>();

        if (tags_equalize_hist)
            CALLCV(cv::equalizeHist(*INPUT, *OUTPUT));
        CALLCV(cv::blur(*INPUT, *OUTPUT, cv::Size(3, 3)));
        //CALLCV(cv::dilate(*INPUT, *OUTPUT, gpu_dilation_element));
        //CALLCV(cv::erode(*INPUT, *OUTPUT, gpu_dilation_element));
        //CALLCV(cv::threshold(*INPUT, *OUTPUT, tags_threshold, 255, cv::THRESH_BINARY));
        CALLCV(cv::adaptiveThreshold(*INPUT, *OUTPUT, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 15, -5));
        CALLCV(cv::subtract(255, *INPUT, *OUTPUT));

#ifdef DEBUG_TAGS
        static const bool show_debug_info = SETTING(tags_debug).value<bool>();
        cv::Mat local;
        if (show_debug_info) {
            INPUT->copyTo(local);
            //INPUT->convertTo(local, CV_8UC1, 255.0);
            tf::imshow("only_bg", local);
        }
#endif

        tags.clear();

        std::vector<std::vector<cv::Vec2i>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(*INPUT, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

#ifdef DEBUG_TAGS
        cv::Mat result;// = cv::Mat::zeros(buffer.rows, buffer.cols, CV_8UC4);
        if (show_debug_info)
            cv::cvtColor(input, result, cv::COLOR_GRAY2BGRA);
#endif
        //size_t found = 0;
        std::mutex mutex;
        using namespace gui;

        distribute_vector([&](auto index, auto start, auto end, auto){
            process_tags(index, start, end, result, hierarchy, contours, INPUT, input, _average, tags);
        }, _contour_pool, contours.begin(), contours.end());

#ifdef DEBUG_TAGS
        if (show_debug_info) {
            //print("found ", found, " tags. ", tags);
            //resize_image(result, 0.5, cv::INTER_LINEAR);
            cv::cvtColor(result, result, cv::COLOR_BGRA2RGBA);
            cv::imwrite("C:/Users/tristan/Videos/result.png", result);
            tf::imshow("result", result);
        }
#endif
    }
#endif
}
