#include "Image.h"
#include <png.h>
#include <misc/colors.h>
#include <misc/stacktrace.h>

//#define IMAGE_DEBUG_MEMORY_ALLOC

namespace cmn {
    std::string Image::toStr() const {
        return "("+Meta::toStr(cols)+"x"+Meta::toStr(rows)+"x"+Meta::toStr(dims)+" "+Meta::toStr(DurationUS{timestamp_t(std::chrono::time_point_cast<std::chrono::microseconds>(clock_::now()).time_since_epoch()) - _timestamp})+" ago)";
    }
    
    Image::Image(const cv::Mat& mat, int index) : Image(mat, index, now()) {}
    Image::Image(const cv::Mat& mat, int index, timestamp_t timestamp)
    {
        create(mat, index, timestamp);
    }

    Image::Image(const gpuMat& mat, int index) : Image(mat, index, now()) {}
    Image::Image(const gpuMat& mat, int index, timestamp_t timestamp)
    {
        create(mat, index, timestamp);
    }

    Image::Image(uint rows, uint cols, uint dims, int index) : Image(rows, cols, dims, index, now()) { }
    Image::Image(uint rows, uint cols, uint dims, int index, timestamp_t timestamp) {
        create(rows, cols, dims, index, timestamp);
    }

    Image::Image(const Image& other, long_t index) : Image(other, index != -1 ? index : other.index(), other.timestamp()) {}
    Image::Image(const Image& other, long_t index, timestamp_t timestamp) : Image(other.rows, other.cols, other.dims, other.data(), index, timestamp) {}

    Image::Image(uint rows, uint cols, uint dims, const uchar* data, int index) : Image(rows, cols, dims, data, index, now()) {}
    Image::Image(uint rows, uint cols, uint dims, const uchar* data, int index, timestamp_t timestamp)
    {
        create(rows, cols, dims, data, index, timestamp);
    }

    void Image::clear() {
        if(_data) {
            _data = nullptr;
            _array_size = 0;
        }
        
        set_index(-1);
        _size = cols = rows = dims = 0;
        _timestamp = 0;
        
        _custom_data = nullptr;
    }

    uchar Image::at(uint y, uint x, uint channel) const {
        assert(y < rows);
        assert(x < cols);
        assert(channel < dims);
        return data()[ptr_safe_t(x) * dims + ptr_safe_t(channel) + ptr_safe_t(y) * ptr_safe_t(cols) * dims];
    }

    uchar* Image::ptr(uint y, uint x) const {
        //assert(y < rows);
        //assert(x < cols);
        assert((ptr_safe_t(x) * dims + ptr_safe_t(y) * ptr_safe_t(cols) * dims) <= size());
        return data() + (ptr_safe_t(x) * dims + ptr_safe_t(y) * ptr_safe_t(cols) * dims);
    }

    void Image::set_pixel(uint x, uint y, const gui::Color& color) const {
        assert(y < rows);
        assert(x < cols);
        
        auto ptr = data() + ptr_safe_t(x) * dims + ptr_safe_t(y) * ptr_safe_t(cols) * dims;
        switch (dims) {
            case 4:
                *(ptr + 3) = color.a;
            case 3:
                *(ptr + 2) = color.b;
            case 2:
                *(ptr + 2) = color.g;
            case 1:
                *(ptr + 2) = color.r;
                
            default:
                break;
        }
    }

#ifdef IMAGE_DEBUG_MEMORY_ALLOC
    Image::~Image() {
        if (_data) {
            print("freeing memory at ", _data, " of size ", _array_size, " and dimensions ", cols, "x", rows);
        }
    }
#endif

    void Image::create(uint rows, uint cols, uint dims, long_t index) {
        create(rows, cols, dims, index, now());
    }
    void Image::create(uint rows, uint cols, uint dims, long_t index, timestamp_t stamp) {
        size_t N = size_t(cols) * size_t(rows) * size_t(dims);

        if (_data) {
            //! if the contained array is either way too big,
            //! does not exist, or is too small... allocate new.
            if (_array_size < N || _array_size > N * 2) {
                if (cols < 128 && rows < 128)
                    _array_size = N * 2;
                else
                    _array_size = N;

#ifdef IMAGE_DEBUG_MEMORY_ALLOC
                print("Realloc of image ",this->cols,"x",this->rows,"x",this->dims," to ",cols,"x",rows,"x",dims," (",_array_size," >= ",N,")");
#endif
                _data.realloc(_array_size * sizeof(uchar));
#ifndef NDEBUG
                //! this does not help us anyway... just crash, i guess.
                if (!_data) FormatExcept("Cannot allocate memory for image of size ",rows,"x",cols,"x",dims,". Leaking.");
#endif
            } // otherwise just be memory-inefficient and leave the array as it is :-)
#ifdef IMAGE_DEBUG_MEMORY_ALLOC
            else
                print("Reusing ", _size," for ",_array_size," array");
#endif
        }
        else if(N > 0) {
            //! no array yet, malloc.
            if (cols < 128 && rows < 128)
                _array_size = N * 2;
            else
                _array_size = N;

            _data.malloc(_array_size * sizeof(uchar));
#ifdef IMAGE_DEBUG_MEMORY_ALLOC
            print("New malloc for ",cols,"x",rows," of size ",_size," at ",_data," (",_array_size," >= ",N,")");
            //print_stacktrace(stdout);
#endif
        }

        //! set data
        this->cols = cols;
        this->rows = rows;
        this->dims = dims;
        this->_index = index;
        this->_timestamp = stamp;
        this->_size = N;
    }

    void Image::create(uint rows, uint cols, uint dims, const uchar* data, long_t index) {
        create(rows, cols, dims, data, index, now());
    }
    void Image::create(uint rows, uint cols, uint dims, const uchar* data, long_t index, timestamp_t stamp) {
        create(rows, cols, dims, index, stamp);
        if(data != nullptr)
            std::memcpy(_data.get(), data, _size);
    }

    void Image::create(const cv::Mat& mat, long_t index) {
        create(mat, index, now());
    }
    void Image::create(const cv::Mat& mat, long_t index, timestamp_t stamp) {
        assert(mat.isContinuous());
        create(mat.rows, mat.cols, mat.channels(), mat.data, index, stamp);
    }

    void Image::create(const gpuMat& mat, long_t index) {
        create(mat, index, now());
    }
    void Image::create(const gpuMat& mat, long_t index, timestamp_t stamp) {
        assert(mat.isContinuous());
        if(not (mat.type() == CV_8UC(mat.channels())))
            throw U_EXCEPTION("Can only use uint8_t images.");
        create(mat.rows, mat.cols, mat.channels());
        mat.copyTo(get());
        set_index(index);
        set_timestamp(stamp);
    }

    void Image::create(const Image& mat, long_t index) {
        create(mat, index, now());
    }
    void Image::create(const Image& mat, long_t index, timestamp_t stamp) {
        create(mat.rows, mat.cols, mat.dims, mat.data(), index, stamp);
    }
    
    void Image::set(Image&& other) {
        clear();
        
        //*this = std::move(other);
        _index = other._index;
        _timestamp = other._timestamp;
        _custom_data = std::move(other._custom_data);
        _data = std::move(other._data);
        cols = other.cols;
        rows = other.rows;
        dims = other.dims;
        _size = other._size;
        _array_size = other._array_size;
        
        other._data = nullptr;
        other._custom_data = nullptr;
        other._size = other._array_size = 0;
        other.cols = other.rows = other.dims = 0;
    }

    void Image::set_custom_data(CustomData *ptr) {
        _custom_data = std::unique_ptr<CustomData>(ptr);
    }

    Image::Image(Image&& other) noexcept
        : _data(std::move(other._data)),
          _size(other._size),
          _array_size(other._array_size),
          _timestamp(other._timestamp),
          _custom_data(std::move(other._custom_data)),
          _index(other._index),
          cols(other.cols),
          rows(other.rows),
          dims(other.dims)
    {
        // Reset the moved-from object
        other._size = 0;
        other._array_size = 0;
        other._timestamp = now();  // Assuming 'now()' is a valid function or macro
        other._index = -1;
        other.cols = 0;
        other.rows = 0;
        other.dims = 0;
    }

    Image& Image::operator=(Image&& other) noexcept {
        // Self-assignment detection
        if (&other == this) {
            return *this;
        }
      
        // Release any resources that *this owns
        _data.nullify();
        _custom_data.reset();
        
        // Move ownership of resources
        _data = std::move(other._data);
        _size = other._size;
        _array_size = other._array_size;
        _timestamp = other._timestamp;
        _custom_data = std::move(other._custom_data);
        _index = other._index;
        cols = other.cols;
        rows = other.rows;
        dims = other.dims;

        // Reset the moved-from object
        other._size = 0;
        other._array_size = 0;
        other._timestamp = now();  // Assuming 'now()' is a valid function or macro
        other._index = -1;
        other.cols = 0;
        other.rows = 0;
        other.dims = 0;

        return *this;
    }

    void Image::set_to(uchar value) {
        std::fill(data(), data()+size(), value);
    }

    void Image::set_channels(const uchar *source, const std::set<uint> &channels) {
        assert(!channels.empty());
#ifndef NDEBUG
        for(auto c : channels)
            assert(c < dims);
#endif
        
        auto data = this->data();
        auto ptr = data;
        auto end = data + size();
        auto m = source;
        for(; ptr<end; ptr+=dims, ++m)
            for(auto c : channels)
                *(ptr + c) = *m;
    }
    
    void Image::set_channel(size_t idx, const uchar* matrix) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto data = this->data();
        auto ptr = data + idx;
        auto m = matrix;
        for(; ptr<data + _size; ptr+=dims, ++m)
            *ptr = *m;
    }
    
    void Image::set_channel(size_t idx, uchar value) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto data = this->data();
        auto ptr = data + idx;
        for(; ptr<data + _size; ptr+=dims)
            *ptr = value;
    }
    
    void Image::set_channel(size_t idx, const std::function<uchar(size_t)>& value) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto data = this->data();
        auto ptr = data + idx;
        size_t i=0;
        for(; ptr<data + _size; ptr+=dims, ++i)
            *ptr = value(i);
    }
    
    void Image::get(cv::Mat& matrix) const {
        assert(int(rows) == matrix.rows && int(cols) == matrix.cols && int(dims) == matrix.channels());
        assert(matrix.isContinuous());
        std::memcpy(matrix.data, data(), _size);
    }
    
    cv::Mat Image::get() const {
        assert(_size == rows * cols * dims * sizeof(uchar));
        return cv::Mat(rows, cols, CV_8UC(dims), data());//, cv::Mat::AUTO_STEP);
    }
    
    struct PNGGuard {
        png_struct *p;
        png_info *info;
        PNGGuard(png_struct *p) : p(p)  {}
        ~PNGGuard() { if(p) {  png_destroy_write_struct(&p, info ? &info : NULL); } }
    };
    
    static void PNGCallback(png_structp  png_ptr, png_bytep data, png_size_t length) {
        std::vector<uchar> *p = (std::vector<uchar>*)png_get_io_ptr(png_ptr);
        p->insert(p->end(), data, data + length);
    }
    
    void to_png(const Image& _input, std::vector<uchar>& output) {
        if(_input.dims < 4 && _input.dims != 1 && _input.dims != 2)
            throw U_EXCEPTION("Currently, only RGBA and GRAY is supported.");
        
        Image::Ptr tmp;
        const Image *input = &_input;
        if (_input.dims == 2) {
            std::vector<cv::Mat> vector;
            cv::split(_input.get(), vector);
            cv::Mat image;
            cv::merge(std::vector<cv::Mat>{vector[0], vector[0], vector[0], vector[1]}, image);
            tmp = Image::Make(image);
            input = tmp.get();
        }
        
        output.clear();
        output.reserve(sizeof(png_byte) * input->cols * input->rows * input->dims);
        
        png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(!p)
            throw U_EXCEPTION("png_create_write_struct() failed");
        PNGGuard PNG(p);
        
        png_infop info_ptr = png_create_info_struct(p);
        if(!info_ptr) {
            throw U_EXCEPTION("png_create_info_struct() failed");
        }
        PNG.info = info_ptr;
        if(0 != setjmp(png_jmpbuf(p)))
            throw U_EXCEPTION("setjmp(png_jmpbuf(p) failed");
        png_set_IHDR(p, info_ptr, input->cols, input->rows, 8,
                     input->dims == 4 ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_GRAY,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);
        png_set_compression_level(p, 1);
        std::vector<uchar*> rows(input->rows);
        for (size_t y = 0; y < input->rows; ++y)
            rows[y] = input->data() + y * input->cols * input->dims;
        png_set_rows(p, info_ptr, rows.data());
        png_set_write_fn(p, &output, PNGCallback, NULL);
        png_write_png(p, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    }
    
    Image::Ptr from_png(const file::Path& path) {
        int width, height;
        png_byte color_type;
        png_byte bit_depth;
        png_bytep *row_pointers;
        
        auto fp = path.fopen("rb");
        
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(!png) abort();
        
        png_infop info = png_create_info_struct(png);
        if(!info) abort();
        
        if(setjmp(png_jmpbuf(png))) abort();
        
        png_init_io(png, fp.get());
        
        png_read_info(png, info);
        
        width      = png_get_image_width(png, info);
        height     = png_get_image_height(png, info);
        color_type = png_get_color_type(png, info);
        bit_depth  = png_get_bit_depth(png, info);
        
        // Read any color_type into 8bit depth, RGBA format.
        // See http://www.libpng.org/pub/png/libpng-manual.txt
        
        if(bit_depth == 16)
            png_set_strip_16(png);
        
        if(color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);
        
        // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
        if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);
        
        if(png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);
        
        // These color_type don't have an alpha channel then fill it with 0xff.
        if(color_type == PNG_COLOR_TYPE_RGB ||
           color_type == PNG_COLOR_TYPE_GRAY ||
           color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
        
        if(color_type == PNG_COLOR_TYPE_GRAY ||
           color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        
        png_read_update_info(png, info);
        
        row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
        for(int y = 0; y < height; y++) {
            row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
        }
        
        png_read_image(png, row_pointers);
        
        static_assert(sizeof(png_byte) == sizeof(uchar), "Must be the same.");
        
        auto ptr = Image::Make(height, width, 4);
        for(int y = 0; y < height; y++) {
            png_bytep row = row_pointers[y];
            memcpy(ptr->data() + y * width * 4, row, width * 4);
            /*for(int x = 0; x < width; x++) {
                png_bytep px = &(row[x * 4]);
                // Do something awesome for each pixel here...
                printf("%4d, %4d = RGBA(%3d, %3d, %3d, %3d)\n", x, y, px[0], px[1], px[2], px[3]);
            }*/
        }
        //memcpy(ptr->data(), row_pointers, height * width * 4);
        
        png_destroy_read_struct(&png, &info, NULL);
        return ptr;
    }
    
    cv::Mat restrict_image_keep_ratio(const Size2& max_size, const cv::Mat& input) {
        using namespace gui;
        Size2 image_size(input);
        if(image_size.width <= max_size.width && image_size.height <= max_size.height)
            return input; // everything is fine
        
        float ratio = image_size.width / image_size.height;
        
        if(image_size.width > max_size.width)
            image_size = Size2(max_size.width, max_size.width / ratio);
            
        if(image_size.height > max_size.height)
            image_size = Size2(max_size.height * ratio, max_size.height);
        
        if(image_size.width > input.cols) {
            image_size.width = input.cols;
            image_size.height = input.cols / ratio;
        }
        
        if(image_size.height > input.rows) {
            image_size.height = input.rows;
            image_size.width = input.rows * ratio;
        }
        
        cv::Mat image;
        cv::resize(input, image, cv::Size(image_size), 0, 0, cv::INTER_AREA);
        return image;
    }

    template<typename MatOut>
    void load_image_to_format(ImageMode color, const cv::Mat& tmp, MatOut& output) {
        if (tmp.cols != output.cols || tmp.rows != output.rows) {
            throw InvalidArgumentException("Cannot convert image to target with different dimensions.");
        }
        uint8_t expected_channels = required_channels(color);
        if (output.channels() != expected_channels) {
            throw InvalidArgumentException("Cannot convert image to target with different number of channels.");
        }

        if (color == ImageMode::R3G3B2) {
            if (output.channels() != 1) {
                throw InvalidArgumentException("Cannot convert image to R3G3B2 into target with ", output.channels(), " channels.");
            }

            if constexpr (are_the_same<decltype(output), cv::Mat>) {
                if (tmp.channels() == 3) {
                    convert_to_r3g3b2<3>(tmp, output);
                    return;
                }
                else if (tmp.channels() == 4) {
                    convert_to_r3g3b2<4>(tmp, output);
                    return;
                }
            }
            else {
                // gpumat
                cv::Mat buffer = cv::Mat::zeros(tmp.size(), CV_8UC(output.channels()));
                if (tmp.channels() == 3) {
                    convert_to_r3g3b2<3>(tmp, buffer);
                    buffer.copyTo(output);
                    return;
                }
                else if (tmp.channels() == 4) {
                    convert_to_r3g3b2<4>(tmp, buffer);
                    buffer.copyTo(output);
                    return;
                }
            }

        }
        else if (tmp.channels() == output.channels()) {
            tmp.copyTo(output);
            return;
        }
        else if (tmp.channels() == 1) {
            switch (output.channels()) {
                // case 1 already dealt with because then channels
                // are the equal between output and input
            case 3:
                cv::cvtColor(tmp, output, cv::COLOR_GRAY2BGR);
                return;
            case 4:
                cv::cvtColor(tmp, output, cv::COLOR_GRAY2BGRA);
                return;

            default:
                break;
            }
        }
        else if (tmp.channels() == 3) {
            if (output.channels() == 4) {
                cv::cvtColor(tmp, output, cv::COLOR_BGR2BGRA);
                return;
            } else if(output.channels() == 1) {
                cv::cvtColor(tmp, output, cv::COLOR_BGR2GRAY);
                return;
            }

        } else if (tmp.channels() == 4) {
            if (output.channels() == 3) {
                cv::cvtColor(tmp, output, cv::COLOR_BGRA2BGR);
                return;
            } else if(output.channels() == 1) {
                cv::cvtColor(tmp, output, cv::COLOR_BGRA2GRAY);
                return;
            }
        }

        throw InvalidArgumentException("Unknown number of channels ", tmp.channels(), " to be converted to ", output.channels(), " in mode ", color, ".");
    }

    template void load_image_to_format<cv::Mat>(ImageMode, const cv::Mat&, cv::Mat&);
    template void load_image_to_format<gpuMat>(ImageMode, const cv::Mat&, gpuMat&);

    template<typename MatOut>
    void load_image_to_format(ImageMode color, const std::string& path, MatOut& output) {
        cv::Mat tmp = cv::imread(path, cv::IMREAD_UNCHANGED);
        if (tmp.empty()) {
            throw std::runtime_error("Failed to open image at path: " + path);
        }
        load_image_to_format(color, tmp, output);
    }

    template void load_image_to_format<cv::Mat>(ImageMode color, const std::string& path, cv::Mat& output);
    template void load_image_to_format<gpuMat>(ImageMode color, const std::string& path, gpuMat& output);
}
