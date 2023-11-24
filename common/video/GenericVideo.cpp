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

void GenericVideo::undistort(const gpuMat& disp, gpuMat &image) const {
    if (GlobalSettings::map().has("cam_undistort") && SETTING(cam_undistort)) {
        static cv::Mat map1;
        static cv::Mat map2;
        if(map1.empty())
            GlobalSettings::get("cam_undistort1").value<cv::Mat>().copyTo(map1);
        if(map2.empty())
            GlobalSettings::get("cam_undistort2").value<cv::Mat>().copyTo(map2);
        
        if(map1.cols == disp.cols && map1.rows == disp.rows && map2.cols == disp.cols && map2.rows == disp.rows)
        {
            if(!map1.empty() && !map2.empty()) {
                print("Undistorting ", disp.cols,"x",disp.rows);
                
                static gpuMat _map1, _map2;
                if(_map1.empty())
                    map1.copyTo(_map1);
                if(_map2.empty())
                    map2.copyTo(_map2);
                
                static gpuMat input;
                disp.copyTo(input);
                
                cv::remap(input, image, _map1, _map2, cv::INTER_LINEAR, cv::BORDER_DEFAULT);
                //output.copyTo(image);
            } else {
                FormatWarning("remap maps are empty.");
            }
        } else {
            FormatError("Undistortion maps are of invalid size (", map1.cols, "x", map1.rows, " vs ", disp.cols, "x", disp.rows, ").");
        }
        
        
        //cv::remap(display, display, map1, map2, cv::INTER_LINEAR, cv::BORDER_DEFAULT);
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

void GenericVideo::generate_average(cv::Mat &av, uint64_t frameIndex, std::function<void(float)>&& callback) {
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
            if(callback)
                callback(float(samples - i.get()) / float(step.get()));
            print("generating average: ", (samples - i.get()) / step.get(),"/", int(samples)," step:", step," (frame ", i,")");
            counted = 0_f;
        }
        
        if(GlobalSettings::has("terminate") && SETTING(terminate))
            break;
    }
    
    auto image = accumulator.finalize();
    image->get().copyTo(av);
}
