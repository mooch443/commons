#ifndef _RAWPROCESSING_H
#define _RAWPROCESSING_H

#include <commons.pc.h>
#include <misc/Blob.h>
#include <misc/GlobalSettings.h>
#include <misc/bid.h>

namespace cmn {
	class RawProcessing;
    class LuminanceGrid;
}

struct TagCache {
    std::vector<pv::BlobPtr> tags;
    
    std::vector<std::vector<cv::Vec2i>> contours;
    std::vector<cv::Vec4i> hierarchy;
};

/**
 * The task of this class is to provide functionality that prepares 
 * raw images for further processing. For example, it converts raw 
 * images into binary images containing only the fish (and some noise).
 * It basically does (or calls) most of the image processing that's 
 * relevant for this program.
 */
class cmn::RawProcessing {
    gpuMat _buffer0, _buffer1, diff;
    gpuMat _floatb0, _floatb1, _polygon;
    gpuMat _grey_average;
    const gpuMat* _average;
    const gpuMat* _float_average;
    
    //const LuminanceGrid *_grid;
    //gpuMat _binary;
    //gpuMat _difference;
    //cv::Mat _hsv;

public:
    RawProcessing(const gpuMat &average, const gpuMat *float_average, const LuminanceGrid* grid);
    ~RawProcessing() {
        _buffer0.release();
        _buffer1.release();
        diff.release();
        _floatb0.release();
        _floatb1.release();
    }

    void generate_binary(const cv::Mat& cpu_input, const gpuMat& input, cv::Mat& output, TagCache*);
    cv::Mat get_binary() const;
};


#endif
