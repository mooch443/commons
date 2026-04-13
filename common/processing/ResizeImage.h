#pragma once

#include <commons.pc.h>

namespace cmn {

enum class ImageResizeMode {
    stretch,
    letterbox
};

struct ImageResizeGeometry {
    Vec2 offset{0.f, 0.f};
    Vec2 scale{1.f, 1.f};
    Size2 content_size{0, 0};
};

inline ImageResizeGeometry resize_image_into(
    const cv::Mat& src,
    Size2 target_size,
    cv::Mat& dst,
    ImageResizeMode mode,
    uchar background = 114)
{
    if(target_size.width <= 0 || target_size.height <= 0) {
        throw U_EXCEPTION("Invalid resize target size ", target_size, ".");
    }
    if(src.empty()) {
        throw U_EXCEPTION("Cannot resize an empty source image.");
    }

    if(dst.cols != target_size.width
       || dst.rows != target_size.height
       || dst.type() != src.type())
    {
        dst.create(target_size.height, target_size.width, src.type());
    }

    if(mode == ImageResizeMode::stretch) {
        cv::resize(src, dst, cv::Size(target_size.width, target_size.height), 0.0, 0.0, cv::INTER_LINEAR);
        return ImageResizeGeometry{
            .offset = Vec2(0.f, 0.f),
            .scale = Vec2(
                static_cast<float>(src.cols) / static_cast<float>(target_size.width),
                static_cast<float>(src.rows) / static_cast<float>(target_size.height)),
            .content_size = target_size
        };
    }

    dst.setTo(cv::Scalar::all(background));

    const double gain = std::min(
        static_cast<double>(target_size.width) / static_cast<double>(src.cols),
        static_cast<double>(target_size.height) / static_cast<double>(src.rows));
    const int resized_width = std::max(1, static_cast<int>(std::round(static_cast<double>(src.cols) * gain)));
    const int resized_height = std::max(1, static_cast<int>(std::round(static_cast<double>(src.rows) * gain)));
    const int pad_x = std::max(0_F, (target_size.width - resized_width) / 2_F);
    const int pad_y = std::max(0_F, (target_size.height - resized_height) / 2_F);

    cv::resize(
        src,
        dst(cv::Rect(pad_x, pad_y, resized_width, resized_height)),
        cv::Size(resized_width, resized_height),
        0.0,
        0.0,
        cv::INTER_LINEAR);

    return ImageResizeGeometry{
        .offset = Vec2(-static_cast<float>(pad_x), -static_cast<float>(pad_y)),
        .scale = Vec2(
            static_cast<float>(src.cols) / static_cast<float>(resized_width),
            static_cast<float>(src.rows) / static_cast<float>(resized_height)),
        .content_size = Size2(resized_width, resized_height)
    };
}

} // namespace cmn
