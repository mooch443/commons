#define _USE_MATH_DEFINES

#include "Video.h"
#include <misc/ThreadPool.h>

#include <opencv2/opencv.hpp>
#if CV_MAJOR_VERSION >= 3
    #include <opencv2/core/ocl.hpp>
#endif

#include <cmath>
#if defined(VIDEOS_USE_CUDA)
    #include "cuda/dynlink_nvcuvid.h"
#endif

#include <misc/Timer.h>
#include <misc/GlobalSettings.h>

using namespace cv;
using namespace cmn;

/**
 * Constructor of @class Video.
 */
Video::Video() : _maps_calculated(false), _cap(NULL), _please_stop(false), _thread(NULL) {
/*#if CV_MAJOR_VERSION >= 3
#ifdef USE_GPU_MAT
    if(cv::cuda::getCudaEnabledDeviceCount() > 0) {
        cv::cuda::setDevice(0);
    }
    
    assert(cv::ocl::haveOpenCL());
    cv::ocl::setUseOpenCL(true);
    
    cv::ocl::Context context;
    if (!context.create(cv::ocl::Device::TYPE_DGPU) || context.ndevices() == 0)
        throw U_EXCEPTION("Cannot create OpenCL context for dedicated GPU.");
    cv::ocl::Device(context.device(0));

#else
    cv::ocl::setUseOpenCL(false);
#endif
#endif*/
}

/**
 * Destructor of @class Video.
 */
Video::~Video() {
    close();
}

/**
 * Opens the file with the given name.
 * @param filename
 * @return bool
 */
bool Video::open(const std::string& filename) {
    close();
    
#if defined(VIDEOS_USE_CUDA)
    d_reader = cv::cudacodec::createVideoReader(filename);
    _size = cv::Size(d_reader->format().width, d_reader->format().height);
#endif

    _cap = new VideoCapture(filename);
    if(!_cap->isOpened()) {
        close();
        return false;
    }
    
#if !defined(VIDEOS_USE_CUDA)
    _size = cv::Size(_cap->get(cv::CAP_PROP_FRAME_WIDTH), _cap->get(cv::CAP_PROP_FRAME_HEIGHT));
#endif

    _last_index = Frame_t{};
    _filename = filename;
    
    return true;
}

void Video::set_colored(ImageMode c) {
    _colored = c;
}

/**
 * Closes the file if opened.
 */
void Video::close() {
    clear();
    
#if defined(VIDEOS_USE_CUDA)
    if(!d_reader.empty())
        d_reader.release();
#endif
    
    if(_cap)
        delete _cap;
    _cap = NULL;
    _last_index = Frame_t{};
}

/**
 * Framerate of the current video.
 * @return int
 */
int Video::framerate() const {
    return (int)_cap->get(cv::CAP_PROP_FPS);
}

/**
 * Length (in frames) of the current video.
 * @return int
 */
Frame_t Video::length() const {
    return Frame_t(sign_cast<Frame_t::number_t>(_cap->get(cv::CAP_PROP_FRAME_COUNT)));
}

/**
 * True if a video is loaded.
 * @return bool
 */
bool Video::isOpened() const {
    return _cap && _cap->isOpened();
}

/**
 * Returns the video dimensions.
 * @return cv::Size
 */
const cv::Size& Video::size() const {
    if(!_cap)
        throw U_EXCEPTION("Trying to retrieve size of a video that has not been opened.");
    return _size;
}

/**
 * Extracts exactly one given channel from a cv::Mat.
 * Assumes the output is set, and mat is a 8UC3.
 */
void extractu8(const cv::Mat& mat, cv::Mat& output, uint channel) {
    if(mat.channels() == 1 || (uint)mat.channels() < channel) {
       mat.copyTo(output);
       return;
    }
    
    static const bool video_reading_use_threads = GlobalSettings::has("video_reading_use_threads") ? SETTING(video_reading_use_threads).value<bool>() : true;
    assert(output.type() == CV_8UC1);
    assert(mat.type() == CV_8UC3);
    const auto channels = mat.channels();
    const auto start = output.data;

    if (video_reading_use_threads
       && size_t(mat.cols) * size_t(mat.rows) >= size_t(1000u) * size_t(1000u))
    {
       static GenericThreadPool pool(cmn::hardware_concurrency(), "extractu8");

       distribute_indexes([&](auto, const auto s, const auto e, auto) {
           for (auto ptr = s; ptr != e; ++ptr) {
               *ptr = mat.data[size_t(ptr - start) * channels + channel];
           }

       }, pool, start, start + uint64_t(output.rows) * uint64_t(output.cols));
    }
    else {
       const auto end = start + uint64_t(output.rows) * uint64_t(output.cols);
       for (auto ptr = start; ptr != end; ++ptr) {
           *ptr = mat.data[size_t(ptr - start) * channels + channel];
       }
    }
}

/**
 * Returns frame 'index' if a video is loaded and it exists.
 *
 * Result is cached.
 * @param index
 * @return cv::Mat
 */
void Video::frame(Frame_t index, cv::Mat& frame, bool lazy, cmn::source_location loc) {
    /*if(_frames.count(index)) {
     return _frames.at(index);
     }*/
    //static Timing timing("Video::frame", 1);
    //TakeTiming take(timing);
    
    if (index >= length())
        throw U_EXCEPTION("Read out of bounds ",index,"/",length(),". (caller ", loc.file_name(), ":", loc.line(),")");
    if (!_cap)
        throw U_EXCEPTION("Video ",_filename," has not yet been loaded.");
    
    // Set position to requested frame
    //if(!_last_index.valid() || index > _last_index + 1000_f)
    //    lazy = true;
    
    auto next_index = _last_index.valid() ? _last_index + 1_f : 0_f;
    if(index != next_index) {
        //FormatWarning("Have to reset video index from ", _last_index," to ", index," (",_filename.c_str(),")");
        int32_t start = index.try_sub(1_f).get();
        _cap->set(cv::CAP_PROP_POS_FRAMES, start);
        int32_t currentPos = _cap->get(cv::CAP_PROP_POS_FRAMES);
        //print("Set to ", start, " and get ", currentPos);
        
        while(start > 0 && currentPos + 1 > index.get()) {
            start = max(0, start - int32_t(currentPos) + int32_t(index.get()) - 1);
            _cap->set(cv::CAP_PROP_POS_FRAMES, start);
            currentPos = _cap->get(cv::CAP_PROP_POS_FRAMES);
            //print("Retrieving ", start, " and get ", currentPos, " (",index,"): ", int32_t(currentPos) - int32_t(index.get()));
        }
        /*if(_cap)
            delete _cap;
        _cap = new cv::VideoCapture(_filename);
        
        int32_t currentPos = 0;*/
        _last_index = Frame_t(currentPos);
        
        for(; _last_index+1_f < index; ++_last_index) {
            _cap->grab();
            //print("* #Skipping ", _last_index, " (",index,")");
        }
    }
    
    //! Read requested frame
    // check whether we already have information on the color
    // channels and dimensions:
    const uint8_t _required_channels = _colored == ImageMode::RGB ? 3 : 1;
    
    if(_channels == 0) {
        if(not _cap->read(read))
            throw U_EXCEPTION("Cannot read frame ",index," of video ",_filename,". (caller ",loc.file_name(), ":", loc.line(), ")");
        
        _channels = read.channels();
        if(_channels == _required_channels) {
            read.copyTo(frame);
        }
        
    } else if(_channels == _required_channels) {
        if(not _cap->read(frame))
            throw U_EXCEPTION("Cannot read (1:1) frame ",index," of video ",_filename,". (caller ",loc.file_name(), ":", loc.line(), ")");
        
    } else if(not _cap->read(read))
        throw U_EXCEPTION("Cannot read frame ",index," of video ",_filename,". (caller ",loc.file_name(), ":", loc.line(), ")");
    
    if(_colored != ImageMode::RGB) {
        if(_channels == _required_channels) {
            // already downloaded
            if(_colored == ImageMode::R3G3B2) {
                throw U_EXCEPTION("Cannot generate R3G3B2 from grayscale video.");
            }
        }
        else if(read.channels() > 1) {
            if(_colored == ImageMode::R3G3B2) {
                convert_to_r3g3b2(read, frame);
            } else {
                static const uint8_t color_channel = SETTING(color_channel).value<uint8_t>();
                if(color_channel >= 3) {
                    // turn into HUE
                    if(read.channels() == 3) {
                        cv::cvtColor(read, read, cv::COLOR_BGR2HSV);
                        extractu8(read, frame, color_channel % 3);
                        
                    } else print("Cannot copy to read frame with ",read.channels()," channels.");
                } else {
                    if(frame.cols != read.cols || frame.rows != read.rows || frame.type() != CV_8UC1) {
                        frame = cv::Mat(read.rows, read.cols, CV_8UC1);
                    }
                    
                    extractu8(read, frame, color_channel);
                }
            }
        } else
            read.copyTo(frame);
    } else if(_channels == _required_channels) {
        // already in the correct image
        if(_colored == ImageMode::R3G3B2) {
            throw U_EXCEPTION("Cannot generate R3G3B2 from grayscale video.");
        }
        
    } else {
        if(read.channels() == 3) {
            read.copyTo(frame);
        } else if(read.channels() == 1) {
            cv::cvtColor(read, frame, cv::COLOR_GRAY2BGR);
        } else
            throw U_EXCEPTION("Unknown color mode.");
    }
    //_frames[index] = frame;
    //return _frames[index];
    
    /*if(not _last_index.valid()) {
        _last_index = index;
    } else _last_index = _last_index + 1_f;*/
    _last_index = index;
    
    //cv::putText(frame, Meta::toStr(_last_index), Vec2(10,20), cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(255,255,255,255));
    //cv::putText(frame, Meta::toStr(_cap->get(cv::CAP_PROP_POS_FRAMES)), Vec2(10,40), cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(255,255,255,255));
    
    //print("_last_index == ", _last_index, " for index ", index, " reading ", _cap->get(cv::CAP_PROP_POS_FRAMES));
}

/**
 * Sets a callback function for video playback. If a new frame is ready, this
 * function will be called and passed a cv::Mat and an integer (frame number).
 * @param f
 */
void Video::onFrame(const frame_callback& f) {
    _frame_callback = f;
}

/**
 * Sets the intrinsic parameters (focal length, skew, etc.) for this video. They
 * are saved inside the object and combined in a camera matrix.
 * @param focal The focal length in pixels for x and y
 * @param alpha_c Skew coefficient
 * @param cc Principal point (usually center of the image)
 * @return Mat33
 */
const Mat33& Video::set_intrinsics(const Mat21& focal, const ScalarType alpha_c, const Mat21& cc) {
    cv::setIdentity(_camera_matrix);
    
    _camera_matrix(0,0) = focal(0,0);
    _camera_matrix(1,1) = focal(1,0);
    
    _camera_matrix(0,1) = focal(0,0) * alpha_c;
    
    _camera_matrix(0,2) = cc(0,0);
    _camera_matrix(1,2) = cc(1,0);
    
    return _camera_matrix;
}

/**
 * Sets the distortion vector for this camera. Used during undistortion.
 * @param distortion Distortion coefficient vector from camera calibration
 * @return Mat51
 */
const Mat51& Video::set_distortion(const Mat51& distortion) {
    _distortion = distortion;
    return _distortion;
}

/**
 * Returns the undistorted version of the frame at given index.
 * @param index
 * @return cv::Mat
 */
const cv::Mat& Video::undistorted_frame(Frame_t index) {
    if(_undistorted_frames.count(index)) {
        return _undistorted_frames[index];
    }
    
    if(!_maps_calculated)
        calculate_undistort_maps();
    
    cv::Mat input;
    gpuMat _input;
    frame(index, input);
    input.copyTo(_input);
    
#ifdef USE_GPU_MAT
    gpuMat output;
    cv::remap(_input, output, _undistort_maps.first, _undistort_maps.second, cv::INTER_LINEAR, cv::BORDER_DEFAULT);
    output.copyTo(_undistorted_frames[index]);
#else
    cv::remap(_input, _undistorted_frames[index], _undistort_maps.first, _undistort_maps.second, cv::INTER_LINEAR, cv::BORDER_DEFAULT);
#endif
    
    return _undistorted_frames[index];
}

/**
 * Returns the camera matrix (skew, focal length, etc. parameters combined).
 * @return Mat33
 */
const Mat33& Video::camera_matrix() const {
    return _camera_matrix;
}

/**
 * Returns the distortion vector as given by the camera calibration.
 * @return Mat51
 */
const Mat51& Video::distortion() const {
    return _distortion;
}

/**
 * Calculates an OpenGL projection matrix based on all given camera parameters.
 *
 * http://kgeorge.github.io/2014/03/08/calculating-opengl-perspective-matrix-from-o
 * encv-intrinsic-matrix/
 * @return Mat44
 */
Mat44 Video::glMatrix() const {
    double zNear = 0.01;
    double zFar = 1000.0;
    
    double width = size().width;
    double height = size().height;
    
    // initialize the projection matrix
    
    // focal length
    double fx = _camera_matrix(0,0);
    double fy = _camera_matrix(1,1);
    
    // camera center
    double cx = _camera_matrix(0,2);
    double cy = _camera_matrix(1,2);
    
    Mat44 projectionMatrix;
    
    // build the final projection matrix
    projectionMatrix(0,0) = 2*fx/width;
    projectionMatrix(0,2) = 1-(2*cx/width);
    projectionMatrix(1,1) = 2*fy/height;
    projectionMatrix(1,2) = -1+((2*cy+2)/height);
    projectionMatrix(2,2) = (zFar+zNear)/(zNear-zFar);
    projectionMatrix(2,3) = (2.0*zFar*zNear)/(zNear-zFar);
    projectionMatrix(3,2) = -1.0;
    
    return projectionMatrix;
}

/**
 * Calculates maps using OpenCVs initUndistortRectifyMap method, which are later
 * used to undistort frames of this video.
 * @return std::pair<cv::Mat, cv::Mat>
 */
const std::pair<gpuMat, gpuMat>& Video::calculate_undistort_maps() {
    gpuMat map1, map2;
    cv::initUndistortRectifyMap(_camera_matrix, _distortion, cv::Mat(), _camera_matrix, size(), CV_32FC1, map1, map2);
    
    _undistort_maps = { map1, map2 };
    _maps_calculated = true;
    
    return _undistort_maps;
}

void Video::clear() {
    _undistorted_frames.clear();
    _frames.clear();
}
