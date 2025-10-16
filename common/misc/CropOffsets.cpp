#include "CropOffsets.h"

namespace cmn {

const std::string& CropOffsets::class_name() {
    static const std::string name = "offsets";
    return name;
}

std::string CropOffsets::toStr() const {
    return Meta::toStr(std::array<float, 4>{left,top,right,bottom});
}

glz::json_t CropOffsets::to_json() const {
    return cvt2json(std::array<float, 4>{left,top,right,bottom});
}

CropOffsets CropOffsets::fromStr(const std::string &str ) {
    return CropOffsets(Meta::fromStr<std::array<float, 4>>(str));
}

CropOffsets::CropOffsets(std::array<float, 4> bounds) : CropOffsets(bounds[0], bounds[1], bounds[2], bounds[3])
{ }

bool CropOffsets::operator==(const CropOffsets & other) const {
    return other.left == left && other.right == right && other.top == top && other.bottom == bottom;
}

CropOffsets::CropOffsets(float l, float t, float r, float b)
    : left(l), top(t), right(r), bottom(b)
{
    assert(left >= 0 && left < 0.5);
    assert(top >= 0 && top < 0.5);
    assert(right >= 0 && right < 0.5);
    assert(bottom >= 0 && bottom < 0.5);
}

Bounds CropOffsets::toPixels(const Size2& dimensions) const {
    return Bounds{
        left * dimensions.width,
        top * dimensions.height,
        (1 - right - left) * dimensions.width,
        (1 - bottom - top) * dimensions.height
    };
}

bool CropOffsets::inside(const Vec2 &point, const Size2 &dimensions) const {
    return toPixels(dimensions).contains(point);
}

std::array<Vec2, 4> CropOffsets::corners(const Size2& dimensions) const {
    auto bounds = toPixels(dimensions);
    return {
        bounds.pos(),
        Vec2(bounds.x + bounds.width, bounds.y),
        bounds.pos() + bounds.size(),
        Vec2(bounds.x, bounds.y + bounds.height)
    };
}

bool CropOffsets::empty() const {
    return left == 0 && top == 0 && right == 0 && bottom == 0;
}

void CropOffsets::apply_copy(const cv::Mat& input, cv::Mat& output) const
{
    if(empty()) {
        output = input; /// reference
        return;
    }
    input(toPixels(Size2(input))).copyTo(output);
}

}
