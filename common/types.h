#ifndef VIDEO_TYPES_H
#define VIDEO_TYPES_H

#ifndef WIN32
    #if __cplusplus <= 199711L
      #error This library needs at least a C++11 compliant compiler
    #endif

    #define TREX_EXPORT
    #define EXPIMP_TEMPLATE

#else
    #ifdef TREX_EXPORTS
        #define TREX_EXPORT __declspec(dllexport)
        #define EXPIMP_TEMPLATE
    #else
        #define TREX_EXPORT __declspec(dllimport)
        #define EXPIMP_TEMPLATE extern
    #endif
#endif

#include <misc/detail.h>

#ifdef _MSC_VER
#include <intrin.h>

DWORD __inline __builtin_ctz(uint32_t value)
{
    DWORD trailing_zero = 0;
    _BitScanForward(&trailing_zero, value);
    return trailing_zero;
}

#endif

/**
 * ======================
 * THREAD-SAFE methods
 * ======================
 */

namespace cmn {
namespace blob {
using line_ptr_t = std::unique_ptr<std::vector<HorizontalLine>>;
using pixel_ptr_t = std::unique_ptr<std::vector<uchar>>;

struct Prediction {
    uint8_t clid{255u}; // up to 254 classes
    uint8_t p{0u}; // [0,255] -> [0,1]
    uint8_t _reserved0, _reserved1;
    
    bool valid() const { return clid < 255u; }
    std::string toStr() const;
    static std::string class_name() { return "Prediction"; }
};

struct Pair {
    blob::line_ptr_t lines;
    blob::pixel_ptr_t pixels;
    uint8_t extra_flags;
    Prediction pred;
    
    Pair() = default;
    Pair(line_ptr_t&& lines, pixel_ptr_t&& pixels, uint8_t extra_flags = 0, Prediction&& pred = {})
        : lines(std::move(lines)),
          pixels(std::move(pixels)),
          extra_flags(extra_flags),
          pred(std::move(pred))
    {}
    /*Pair& operator=(Pair&& other) {
        if(!lines) {
            lines = std::move(other.lines);
            pixels = std::move(other.pixels);
        } else {
            *lines = std::move(*other.lines);
            *pixels = std::move(*other.pixels);
        }
        return *this;
    }*/
};
}

typedef std::vector<blob::Pair> blobs_t;
constexpr int CV_MAX_THICKNESS = 32767;
}


#define DEBUG_CV(COMMAND) try { COMMAND; } catch(const cv::Exception& e) { FormatExcept("OpenCV Exception ('", __FILE__ ,"':", __LINE__ ,"): ", #COMMAND ,"\n", e.what()); }

namespace tf {
    void imshow(const std::string& name, const cv::Mat& mat, std::string label = "");
    void show();
    void waitKey(std::string name);
}

namespace gui {
    using namespace cmn;
}

namespace track {
    using namespace cmn;
}

#endif
