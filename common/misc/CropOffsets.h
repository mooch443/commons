#pragma once

#include <commons.pc.h>

namespace cmn {

class CropOffsets {
    float left, top, right, bottom;
    
public:
    CropOffsets(std::array<float, 4> = {0.f,0.f,0.f,0.f});
    CropOffsets(float l, float t, float r, float b);
    
    Bounds toPixels(const Size2& dimensions) const;
    bool inside(const Vec2& point, const Size2& dimensions) const;
    std::array<Vec2, 4> corners(const Size2& dimensions) const;
    bool empty() const;
    
    //! makes a contiguous matrix copy using the coordinates
    //! or does not copy at all if its empty (reference).
    void apply_copy(const cv::Mat& input, cv::Mat& output) const;
    
    std::string toStr() const;
    glz::json_t to_json() const;
    static CropOffsets fromStr(const std::string&);
    static const std::string& class_name();
    bool operator==(const CropOffsets&) const;
};

}
