#ifndef _GENERIC_VIDEO_H
#define _GENERIC_VIDEO_H

#include <commons.pc.h>
#include <misc/CropOffsets.h>
#include <misc/GlobalSettings.h>
#include <misc/frame_t.h>

namespace cmn {

//! Interface for things that can load Videos
class GenericVideo {
    gpuMat map1, map2;
    
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
    
    void undistort(const gpuMat& disp, gpuMat& image);
    void undistort(const cv::Mat& disp, cv::Mat& image);
    
    virtual void set_offsets(const CropOffsets&) {
        throw U_EXCEPTION("Not implemented.");
    }
    
    void processImage(const gpuMat& disp, gpuMat& out, bool do_mask = true) const;
    virtual void generate_average(cv::Mat &average, uint64_t frameIndex, std::function<bool(float)>&& callback = nullptr);
    
    virtual void set_undistortion(
           std::optional<std::vector<double>> &&cam_matrix,
           std::optional<std::vector<double>> &&undistort_vector)
    {
        if(not cam_matrix
           || not undistort_vector)
        {
            map1.release();
            map2.release();
            assert(map1.empty() && map2.empty());
            return;
        }
        
        initialize_undistort(size(),
                             std::move(cam_matrix.value()),
                             std::move(undistort_vector.value()),
                             map1, map2);
    }
    
    template<typename Mat>
    static void initialize_undistort(const Size2& size,
                                     std::vector<double> cam_data,
                                     std::vector<double> undistort_data,
                                     Mat& cam_undistort1,
                                     Mat& cam_undistort2)
    {
        
        if(cam_data.size() != 3u * 3u)
            throw InvalidArgumentException("Camera matrix has to consist of 3x3 doubles.");
        if(undistort_data.size() != 1u * 5u)
            throw InvalidArgumentException("Undistortion matrix has to consist of 1x5 doubles.");
        
        auto cam_matrix = cv::Mat(3, 3, CV_64FC1, cam_data.data());
        auto cam_undistort_vector = cv::Mat(1, 5, CV_64FC1, undistort_data.data());
        
        // Adjust the camera matrix to the scale of the new image
        cam_matrix.at<double>(0, 0) *= size.width; // Scale fx
        cam_matrix.at<double>(1, 1) *= size.height; // Scale fy
        cam_matrix.at<double>(0, 2) *= size.width; // Scale cx (principal point x-coordinate)
        cam_matrix.at<double>(1, 2) *= size.height; // Scale cy (principal point y-coordinate)

        auto drawtransform = cv::getOptimalNewCameraMatrix(cam_matrix, cam_undistort_vector, size, 1.0, size);
        //print_mat("draw_transform", drawtransform);
        //print_mat("cam_matrix", cam_matrix);
        //print_mat("cam_undistort_vector", cam_undistort_vector);
        
        cv::initUndistortRectifyMap(cam_matrix,
                                    cam_undistort_vector,
                                    cv::Mat(),
                                    drawtransform,
                                    size,
                                    CV_32FC1,
                                    cam_undistort1, cam_undistort2);
    }
};

}

#endif
