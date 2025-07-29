#include "GenericVideo.h"
#include <misc/Timer.h>
#include <misc/GlobalSettings.h>
#include <misc/Image.h>
#include <video/AveragingAccumulator.h>

namespace cmn {

CropOffsets GenericVideo::crop_offsets() const {
    return SETTING(crop_offsets);
}

void GenericVideo::undistort(const cv::Mat &input, cv::Mat &output)
{
    if(map1.empty() || map2.empty())
    {
        return;
    }
    
    if(map1.cols == input.cols
       && map1.rows == input.rows
       && map2.cols == input.cols
       && map2.rows == input.rows)
    {
        if(!map1.empty() && !map2.empty()) {
            //Print("Undistorting ", input.cols,"x",input.rows);
            cv::remap(input, output, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        } else {
            FormatWarning("remap maps are empty.");
        }
    } else {
        FormatError("Undistortion maps are of invalid size (", map1.cols, "x", map1.rows, " vs ", input.cols, "x", input.rows, ").");
    }
}

void GenericVideo::undistort(const gpuMat& disp, gpuMat &image)
{
    if(map1.empty() || map2.empty())
        return;
    
    if(map1.cols == disp.cols
       && map1.rows == disp.rows
       && map2.cols == disp.cols
       && map2.rows == disp.rows)
    {
        if(!map1.empty() && !map2.empty()) {
            //Print("Undistorting ", disp.cols,"x",disp.rows);
            cv::remap(disp, image, map1, map2, cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        } else {
            FormatWarning("remap maps are empty.");
        }
    } else {
        FormatError("Undistortion maps are of invalid size (", map1.cols, "x", map1.rows, " vs ", disp.cols, "x", disp.rows, ").");
    }
}

void GenericVideo::processImage(const gpuMat& display, gpuMat&out, bool do_mask) const {
    static Timing timing("processImage");
    timing.start_measure();
    
    gpuMat use;
    assert(is_in(display.channels(), 1, 3));
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
    
    Print("Generating average for frame ", frameIndex," (method='",accumulator.mode().name(),"')...");
    
    float samples = GlobalSettings::read_value_with_default<uint32_t>("average_samples", length().get() / 10u);
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
            Print("generating average: ", (samples - i.get()) / step.get(),"/", int(samples)," step:", step," (frame ", i,")");
            counted = 0_f;
        }
        
        if(GlobalSettings::read_value_with_default<bool>("terminate", false))
            break;
    }
    
    auto image = accumulator.finalize();
    image->get().copyTo(av);
}

}
