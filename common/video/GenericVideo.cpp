#include "GenericVideo.h"
#include <misc/Timer.h>
#include <misc/GlobalSettings.h>
#include <misc/Image.h>
#include <misc/checked_casts.h>
#include <video/AveragingAccumulator.h>

using namespace cmn;

CropOffsets GenericVideo::crop_offsets() const {
    return SETTING(crop_offsets);
}

void GenericVideo::undistort(const cv::Mat &input, cv::Mat &output) {
    if (not GlobalSettings::map().has("cam_undistort")
        || not SETTING(cam_undistort))
    {
        return;
    }
    
    static cv::Mat map1;
    static cv::Mat map2;
    if(map1.empty())
        GlobalSettings::get("cam_undistort1").value<cv::Mat>().copyTo(map1);
    if(map2.empty())
        GlobalSettings::get("cam_undistort2").value<cv::Mat>().copyTo(map2);
    
    if(map1.cols == input.cols
       && map1.rows == input.rows
       && map2.cols == input.cols
       && map2.rows == input.rows)
    {
        if(!map1.empty() && !map2.empty()) {
            print("Undistorting ", input.cols,"x",input.rows);
            cv::remap(input, output, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        } else {
            FormatWarning("remap maps are empty.");
        }
    } else {
        FormatError("Undistortion maps are of invalid size (", map1.cols, "x", map1.rows, " vs ", input.cols, "x", input.rows, ").");
    }
}

void GenericVideo::undistort(const gpuMat& disp, gpuMat &image) {
    if (GlobalSettings::map().has("cam_undistort") && SETTING(cam_undistort)) {
        static gpuMat map1;
        static gpuMat map2;
        if(map1.empty())
            GlobalSettings::get("cam_undistort1").value<cv::Mat>().copyTo(map1);
        if(map2.empty())
            GlobalSettings::get("cam_undistort2").value<cv::Mat>().copyTo(map2);
        
        if(map1.cols == disp.cols 
           && map1.rows == disp.rows
           && map2.cols == disp.cols
           && map2.rows == disp.rows)
        {
            if(!map1.empty() && !map2.empty()) {
                print("Undistorting ", disp.cols,"x",disp.rows);
                cv::remap(disp, image, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
            } else {
                FormatWarning("remap maps are empty.");
            }
        } else {
            FormatError("Undistortion maps are of invalid size (", map1.cols, "x", map1.rows, " vs ", disp.cols, "x", disp.rows, ").");
        }
    }
}

void GenericVideo::processImage(const gpuMat& display, gpuMat&out, bool do_mask) const {
    static Timing timing("processImage");
    timing.start_measure();
    
    gpuMat use;
    assert(display.channels() == 1);
    use = display;
    
    if (has_mask() && do_mask) {
        if(this->mask().rows == use.rows && this->mask().cols == use.cols) {
            out = use.mul(this->mask());
        } else {
            const auto offsets = crop_offsets();
            out = use(offsets.toPixels(Size2(display.cols, display.rows))).mul(this->mask());
        }
    } else {
        use.copyTo(out);
    }
    
    timing.conclude_measure();
}

void GenericVideo::generate_average(cv::Mat &av, uint64_t frameIndex, std::function<bool(float)>&& callback) {
    if(length() < 10_f) {
        gpuMat average;
        av.copyTo(average);
        this->processImage(average, average);
        return;
    }
    AveragingAccumulator accumulator;
    
    print("Generating average for frame ", frameIndex," (method='",accumulator.mode().name(),"')...");
    
    float samples = GlobalSettings::has("average_samples") ? (float)SETTING(average_samples).value<uint32_t>() : (length().get() * 0.1f);
    const Frame_t step = Frame_t(narrow_cast<uint>(max(1, length().get() / samples)));
    
    Image f;
    Frame_t counted = 0_f;
    for(Frame_t i=length() > 0_f ? length()-1_f : 0_f; i>=0_f; i-=step) {
        frame(i, f);
        
        assert(f.dims == 1);
        accumulator.add(f.get());
        counted += step;
        
        if(counted.get() > float(length().get()) * 0.1) {
            if(callback) {
                if(not callback(float(samples - i.get()) / float(step.get()))) {
                    break; // cancel requested by callback
                }
            }
            print("generating average: ", (samples - i.get()) / step.get(),"/", int(samples)," step:", step," (frame ", i,")");
            counted = 0_f;
        }
        
        if(GlobalSettings::has("terminate") && SETTING(terminate))
            break;
    }
    
    auto image = accumulator.finalize();
    image->get().copyTo(av);
}

void GenericVideo::initialize_undistort(const Size2& size) {
    cv::Mat map1, map2;
    
    auto cam_data = SETTING(cam_matrix).value<std::vector<float>>();
    cv::Mat cam_matrix = cv::Mat(3, 3, CV_32FC1, cam_data.data());
    
    auto undistort_data = SETTING(cam_undistort_vector).value<std::vector<float>>();
    cv::Mat cam_undistort_vector = cv::Mat(1, 5, CV_32FC1, undistort_data.data());
    
    // Adjust the camera matrix to the scale of the new image
    cam_matrix.at<float>(0, 0) *= size.width; // Scale fx
    cam_matrix.at<float>(1, 1) *= size.height; // Scale fy
    cam_matrix.at<float>(0, 2) *= size.width; // Scale cx (principal point x-coordinate)
    cam_matrix.at<float>(1, 2) *= size.height; // Scale cy (principal point y-coordinate)

    cv::Mat drawtransform = cv::getOptimalNewCameraMatrix(cam_matrix, cam_undistort_vector, size, 1.0, size);
    print_mat("draw_transform", drawtransform);
    print_mat("cam", cam_matrix);
    //drawtransform = SETTING(cam_matrix).value<cv::Mat>();
    cv::initUndistortRectifyMap(
                                cam_matrix,
                                cam_undistort_vector,
                                cv::Mat(),
                                drawtransform,
                                size,
                                CV_32FC1,
                                map1, map2);
    
    GlobalSettings::map()["cam_undistort1"] = map1;
    GlobalSettings::map()["cam_undistort2"] = map2;
}
