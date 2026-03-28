#include "ImageIO.h"

#include <png.h>

namespace cmn::file {

namespace {

struct PNGReadGuard {
    png_structp png{nullptr};
    png_infop info{nullptr};

    ~PNGReadGuard() {
        if (png) {
            png_destroy_read_struct(&png, &info, nullptr);
        }
    }
};

struct PNGWriteGuard {
    png_structp png{nullptr};
    png_infop info{nullptr};

    explicit PNGWriteGuard(png_structp ptr)
        : png(ptr)
    {}

    ~PNGWriteGuard() {
        if (png) {
            png_destroy_write_struct(&png, info ? &info : nullptr);
        }
    }
};

void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto* output = static_cast<std::vector<uchar>*>(png_get_io_ptr(png_ptr));
    output->insert(output->end(), data, data + length);
}

}

Image::Ptr from_png(const Path& path) {
    auto fp = path.fopen("rb");

    PNGReadGuard guard;
    guard.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!guard.png) {
        abort();
    }

    guard.info = png_create_info_struct(guard.png);
    if (!guard.info) {
        abort();
    }

    if (setjmp(png_jmpbuf(guard.png))) {
        abort();
    }

    png_init_io(guard.png, fp.get());
    png_read_info(guard.png, guard.info);

    const int width = png_get_image_width(guard.png, guard.info);
    const int height = png_get_image_height(guard.png, guard.info);
    const png_byte color_type = png_get_color_type(guard.png, guard.info);
    const png_byte bit_depth = png_get_bit_depth(guard.png, guard.info);

    if (bit_depth == 16) {
        png_set_strip_16(guard.png);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(guard.png);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(guard.png);
    }

    if (png_get_valid(guard.png, guard.info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(guard.png);
    }

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_filler(guard.png, 0xFF, PNG_FILLER_AFTER);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(guard.png);
    }

    png_read_update_info(guard.png, guard.info);

    std::vector<png_bytep> row_pointers(static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        row_pointers[static_cast<size_t>(y)] =
            static_cast<png_bytep>(malloc(png_get_rowbytes(guard.png, guard.info)));
    }

    png_read_image(guard.png, row_pointers.data());

    static_assert(sizeof(png_byte) == sizeof(uchar), "Must be the same.");

    auto image = Image::Make(height, width, 4);
    for (int y = 0; y < height; ++y) {
        const png_bytep row = row_pointers[static_cast<size_t>(y)];
        memcpy(image->data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4, row, static_cast<size_t>(width) * 4);
        free(row_pointers[static_cast<size_t>(y)]);
    }

    return image;
}

void to_png(const Image& input_image, std::vector<uchar>& output) {
    if (input_image.dims < 4 && input_image.dims != 1 && input_image.dims != 2) {
        throw U_EXCEPTION("Currently, only RGBA and GRAY is supported.");
    }

    Image::Ptr converted;
    const Image* input = &input_image;
    if (input_image.dims == 2) {
        std::vector<cv::Mat> channels;
        cv::split(input_image.get(), channels);
        cv::Mat image;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[0], channels[0], channels[1]}, image);
        converted = Image::Make(image);
        input = converted.get();
    }

    output.clear();
    output.reserve(sizeof(png_byte) * input->cols * input->rows * input->dims);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        throw U_EXCEPTION("png_create_write_struct() failed");
    }
    PNGWriteGuard guard(png);

    png_infop info = png_create_info_struct(png);
    if (!info) {
        throw U_EXCEPTION("png_create_info_struct() failed");
    }
    guard.info = info;

    if (setjmp(png_jmpbuf(png)) != 0) {
        throw U_EXCEPTION("setjmp(png_jmpbuf(png)) failed");
    }

    png_set_IHDR(png,
                 info,
                 input->cols,
                 input->rows,
                 8,
                 input->dims == 4 ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_GRAY,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, 1);

    std::vector<uchar*> rows(input->rows);
    for (size_t y = 0; y < input->rows; ++y) {
        rows[y] = input->data() + y * input->cols * input->dims;
    }

    png_set_rows(png, info, rows.data());
    png_set_write_fn(png, &output, png_write_callback, nullptr);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
}

}
