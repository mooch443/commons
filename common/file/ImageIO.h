#pragma once

#include <misc/Image.h>
#include <misc/Path.h>

namespace cmn::file {

void to_png(const Image& input, std::vector<uchar>& output);
Image::Ptr from_png(const Path& path);

}
