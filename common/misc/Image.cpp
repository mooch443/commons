#include "Image.h"
#include <png.h>
#include <misc/metastring.h>
#include <gui/colors.h>
#include <misc/stacktrace.h>

//#define IMAGE_DEBUG_MEMORY_ALLOC

namespace cmn {
    std::string Image::toStr() const {
        return "("+Meta::toStr(cols)+"x"+Meta::toStr(rows)+"x"+Meta::toStr(dims)+" "+Meta::toStr(DurationUS{timestamp_t(std::chrono::time_point_cast<std::chrono::microseconds>(clock_::now()).time_since_epoch()) - _timestamp})+" ago)";
    }
    
    Image::Image()
    {
        set_index(-1);
        reset_stamp();
    }

    Image::Image(const cv::Mat& mat, int index) : Image(mat, index, now()) {}
    Image::Image(const cv::Mat& mat, int index, timestamp_t timestamp)
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
            free(_data);
            _data = NULL;
            _array_size = 0;
        }
        
        set_index(-1);
        _size = cols = rows = dims = 0;
        _timestamp = 0;
        
        if(_custom_data) {
            delete _custom_data;
            _custom_data = nullptr;
        }
    }

    uchar Image::at(uint y, uint x, uint channel) const {
        assert(y < rows);
        assert(x < cols);
        assert(channel < dims);
        return data()[x * dims + channel + y * cols * dims];
    }

    uchar* Image::ptr(uint y, uint x) const {
        assert(y < rows);
        assert(x < cols);
        return data() + (x * dims + y * cols * dims);
    }

    void Image::set_pixel(uint x, uint y, const gui::Color& color) const {
        assert(y < rows);
        assert(x < cols);
        
        auto ptr = data() + x * dims + y * cols * dims;
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

    Image::~Image() {
        if(_data)
            free(_data);
        if(_custom_data)
            delete _custom_data;
    }

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
                _data = (uchar*)realloc(_data, _array_size);
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
        else {
            //! no array yet, malloc.
            if (cols < 128 && rows < 128)
                _array_size = N * 2;
            else
                _array_size = N;

            _data = (uchar*)malloc(_array_size);
#ifdef IMAGE_DEBUG_MEMORY_ALLOC
            print("New malloc for ",cols,"x",rows," of size ",_size," at ",_data," (",_array_size," >= ",N,")");
            print_stacktrace(stdout);
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
            std::memcpy(_data, data, _size);
    }

    void Image::create(const cv::Mat& mat, long_t index) {
        create(mat, index, now());
    }
    void Image::create(const cv::Mat& mat, long_t index, timestamp_t stamp) {
        assert(mat.isContinuous());
        create(mat.rows, mat.cols, mat.channels(), mat.data, index, stamp);
    }

    void Image::create(const Image& mat, long_t index) {
        create(mat, index, now());
    }
    void Image::create(const Image& mat, long_t index, timestamp_t stamp) {
        create(mat.rows, mat.cols, mat.dims, mat.data(), index, stamp);
    }
    
    void Image::set(Image&& other) {
        std::swap(other._index, _index);
        std::swap(other._timestamp, _timestamp);
        std::swap(other._custom_data, _custom_data);
        std::swap(other._data, _data);
        std::swap(other.cols, cols);
        std::swap(other.rows, rows);
        std::swap(other.dims, dims);
        std::swap(other._size, _size);
        std::swap(other._array_size, _array_size);
    }
    
    /*Image& Image::operator=(const Image& other) {
        if (&other == this)
            return *this;
        
        assert(_size == other.size());
        
        _index = other.index();
        _timestamp = other.timestamp();
        if(other._custom_data)
            throw U_EXCEPTION("Cannot copy custom data from one image to another.");
        
        if(_data)
            std::memcpy(_data, other.data(), _size);
        return *this;
    }*/
    
    /*Image& Image::operator=(const cv::Mat& matrix) {
        set(_index, matrix);
        reset_stamp();
        return *this;
    }*/

    /*void Image::operator=(Image &&image) {
        if(image.cols == cols && image.rows == rows && image.dims == dims && _data)
        {
            std::swap(image._data, _data);
            _timestamp = image._timestamp;
            _index = image._index;
        } else {
            set(image.index(), image.get(), image.timestamp());
        }
        
#ifndef NDEBUG
        if(image._custom_data)
            FormatWarning("Cannot copy custom data from one image to another.");
#endif
    }*/
    
    /*void Image::set(long_t idx, const cv::Mat& matrix, timestamp_t stamp) {
        assert(int(rows) == matrix.rows);
        assert(int(cols) == matrix.cols);
        assert(int(dims) == matrix.channels());
        assert(matrix.isContinuous());
        
        if(!_data && _size) {
            create(matrix);
            return;
        }
        
        _index = idx;
        _timestamp = stamp;
        //reset_stamp();
        if(_size)
            std::memcpy(_data, matrix.data, _size);
    }
    
    void Image::set(long_t idx, const uchar* matrix, timestamp_t stamp) {
        assert(_data);
        assert(matrix);
        
        _index = idx;
        _timestamp = stamp;
        std::memcpy(_data, matrix, _size);
    }*/

    void Image::set_to(uchar value) {
        std::fill(data(), data()+size(), value);
    }

    void Image::set_channels(const uchar *source, const std::set<uint> &channels) {
        assert(!channels.empty());
#ifndef NDEBUG
        for(auto c : channels)
            assert(c < dims);
#endif
        
        auto ptr = _data;
        auto end = _data + size();
        auto m = source;
        for(; ptr<end; ptr+=dims, ++m)
            for(auto c : channels)
                *(ptr + c) = *m;
    }
    
    void Image::set_channel(size_t idx, const uchar* matrix) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto ptr = _data + idx;
        auto m = matrix;
        for(; ptr<_data + _size; ptr+=dims, ++m)
            *ptr = *m;
    }
    
    void Image::set_channel(size_t idx, uchar value) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto ptr = _data + idx;
        for(; ptr<_data + _size; ptr+=dims)
            *ptr = value;
    }
    
    void Image::set_channel(size_t idx, const std::function<uchar(size_t)>& value) {
        assert(_data && idx < dims);
        reset_stamp();
        
        auto ptr = _data + idx;
        size_t i=0;
        for(; ptr<_data + _size; ptr+=dims, ++i)
            *ptr = value(i);
    }
    
    void Image::get(cv::Mat& matrix) const {
        assert(int(rows) == matrix.rows && int(cols) == matrix.cols && int(dims) == matrix.channels());
        assert(matrix.isContinuous());
        std::memcpy(matrix.data, _data, _size);
    }
    
    cv::Mat Image::get() const {
        assert(_size == rows * cols * dims * sizeof(uchar));
        return cv::Mat(rows, cols, CV_8UC(dims), _data);//, cv::Mat::AUTO_STEP);
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
        
        Image::UPtr tmp;
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
    
    Image::UPtr from_png(const file::Path& path) {
        int width, height;
        png_byte color_type;
        png_byte bit_depth;
        png_bytep *row_pointers;
        
        FILE *fp = path.fopen("rb");
        
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if(!png) abort();
        
        png_infop info = png_create_info_struct(png);
        if(!info) abort();
        
        if(setjmp(png_jmpbuf(png))) abort();
        
        png_init_io(png, fp);
        
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
        
        fclose(fp);
        
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
}
