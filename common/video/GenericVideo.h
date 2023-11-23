#ifndef _GENERIC_VIDEO_H
#define _GENERIC_VIDEO_H

#include <types.h>
#include <misc/CropOffsets.h>
#include <misc/GlobalSettings.h>
#include <misc/frame_t.h>

namespace cmn {

//! Interface for things that can load Videos
class GenericVideo {
public:
    virtual cv::Mat frame(Frame_t frameIndex, cmn::source_location loc = cmn::source_location::current()) {
        gpuMat image;
        frame(frameIndex, image, loc);
        cv::Mat dl;
        image.copyTo(dl);
        return dl;
    }
    
    virtual bool frame(Frame_t frameIndex, Image& output, cmn::source_location loc = cmn::source_location::current()) = 0;
    #ifdef USE_GPU_MAT
    virtual void frame(Frame_t globalIndex, gpuMat& output, cmn::source_location loc = cmn::source_location::current()) = 0;
    #endif
    
    virtual const cv::Size& size() const = 0;
    virtual Frame_t length() const = 0;
    virtual bool supports_multithreads() const = 0;
    virtual const cv::Mat& average() const = 0;
    virtual bool has_mask() const = 0;
    virtual const cv::Mat& mask() const = 0;
    virtual bool has_timestamps() const = 0;
    virtual timestamp_t timestamp(Frame_t, cmn::source_location loc = cmn::source_location::current()) const {
        throw _U_EXCEPTION<FormatterType::UNIX, const char*>(loc, "Not implemented.");
    }
    virtual timestamp_t start_timestamp() const {
        throw U_EXCEPTION("Not implemented.");
    }
    virtual short framerate() const = 0;
    
public:
    virtual ~GenericVideo() {}
    virtual CropOffsets crop_offsets() const;
    
    void undistort(const gpuMat& disp, gpuMat& image) const;
    
    virtual void set_offsets(const CropOffsets&) {
        throw U_EXCEPTION("Not implemented.");
    }
    
    void processImage(const gpuMat& disp, gpuMat& out, bool do_mask = true) const;
    virtual void generate_average(cv::Mat &average, uint64_t frameIndex, std::function<void(float)>&& callback = nullptr);
};

}

#endif
