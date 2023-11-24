#pragma once

#include <misc/defines.h>
#include <misc/vec2.h>
#include <file/Path.h>
#include <misc/checked_casts.h>
#include <gui/colors.h>

namespace cmn {
    class ReallocDeleter {
    public:
        void operator()(void* ptr) const {
            if (ptr) {
                std::free(ptr);
            }
        }
        
        static void* realloc(void* ptr, std::size_t new_size) {
            return std::realloc(ptr, new_size);
        }
        
        static void* malloc(std::size_t size) {
            return std::malloc(size);
        }
    };

    template<typename T>
    class UniqueReallocPtr {
    public:
        UniqueReallocPtr() : ptr_(nullptr, ReallocDeleter()) {}
        explicit UniqueReallocPtr(T* raw_ptr) : ptr_(raw_ptr, ReallocDeleter()) {}

        T* get() const {
            return static_cast<T*>(ptr_.get());
        }

        void reset(T* new_ptr = nullptr) {
            ptr_.reset(new_ptr);
        }

        void release() {
            ptr_.release();
        }

        void realloc(std::size_t new_size) {
            T* new_ptr = static_cast<T*>(ReallocDeleter::realloc(ptr_.get(), new_size));
            if (new_ptr) {
                ptr_.release();
                ptr_.reset(new_ptr);
            }
        }

        void malloc(std::size_t size) {
            T* new_ptr = static_cast<T*>(ReallocDeleter::malloc(size));
            if (new_ptr) {
                ptr_.reset(new_ptr);
            }
        }

        void nullify() {
            reset(nullptr);
        }
            
        UniqueReallocPtr& operator=(std::nullptr_t) {
            ptr_ = nullptr;
            return *this;
        }

        bool operator==(const UniqueReallocPtr&) const noexcept = default;
        bool operator!=(const UniqueReallocPtr&) const noexcept = default;
        bool operator==(std::nullptr_t) const noexcept {
            return ptr_ == nullptr;
        }
        operator bool() const noexcept { return ptr_ != nullptr; }

    private:
        std::unique_ptr<void, ReallocDeleter> ptr_;
    };

    /**
     * A structure that maps from custom malloc() calls to cv::Mat and back.
     * Also saves an "ID" or index of the image.
     */
    class Image final {
    public:
        typedef std::chrono::steady_clock clock_;
        typedef std::chrono::duration<double, std::ratio<1> > second_;
        using Ptr  = std::unique_ptr<Image>;
        using SPtr = std::shared_ptr<Image>;
        
        template<typename... Args>
        static Ptr Make(Args&&...args) {
            return std::make_unique<Image>(std::forward<Args>(args)...);
        }
        
        class CustomData {
        public:
            virtual ~CustomData() {}
        };
        
    protected:
        UniqueReallocPtr<uchar> _data;
        GETTER_I(size_t, size, 0)
        GETTER_I(size_t, array_size, 0)
        GETTER_SETTER_I(timestamp_t, timestamp, now())
        std::unique_ptr<CustomData> _custom_data;
        GETTER_SETTER_I(long_t, index, -1)
        
    public:
        uint cols = 0, rows = 0, dims = 0;
        auto data() const { return _data.get(); }
        const CustomData* custom_data() const { return _custom_data.get(); }
        CustomData* custom_data() { return _custom_data.get(); }
        void set_custom_data(CustomData* ptr);
        
    public:
        Image() = default;
        Image(Image&&) noexcept;

        Image& operator=(Image&&) noexcept;
        Image& operator=(const Image& other) noexcept = delete;
        
#ifdef IMAGE_DEBUG_MEMORY_ALLOC
        ~Image();
#endif
        
        Image(const Image& other, long_t index = -1);
        Image(const Image& other, long_t index, timestamp_t timestamp);
        Image(uint rows, uint cols, uint dims, const uchar* datat, int index, timestamp_t timestamp);
        Image(uint rows, uint cols, uint dims, const uchar* data, int index = -1);
        Image(uint rows, uint cols, uint dims = 1, int index = -1);

        Image(uint rows, uint cols, uint dims, int index, timestamp_t timestamp);
        explicit Image(const cv::Mat& mat, int index = -1);
        explicit Image(const gpuMat& mat, int index = -1);
        explicit Image(const cv::Mat& mat, int index, timestamp_t timestamp);
        explicit Image(const gpuMat& mat, int index, timestamp_t timestamp);
        
    public:
        void create(uint rows, uint cols, uint dims, long_t index = -1);
        void create(uint rows, uint cols, uint dims, long_t index, timestamp_t stamp);

        void create(uint rows, uint cols, uint dims, const uchar* data, long_t index = -1);
        void create(uint rows, uint cols, uint dims, const uchar* data, long_t index, timestamp_t stamp);

        void create(const cv::Mat& mat, long_t index = -1);
        void create(const cv::Mat& mat, long_t index, timestamp_t stamp);
        
        void create(const gpuMat& mat, long_t index = -1);
        void create(const gpuMat& mat, long_t index, timestamp_t stamp);

        void create(const Image& other, long_t index = -1);
        void create(const Image& other, long_t index, timestamp_t stamp);

        void clear();
        
        void set(Image&&);
        
        //! copy one channel from a 1-d matrix of equal size
        void set_channel(size_t idx, const uchar* matrix);
        
        //! set a whole channel of the image to a constant value
        void set_channel(size_t idx, uchar value);
        //! set whole image to constant value
        void set_to(uchar value);
        
        //! set a whole channel with a generative function
        void set_channel(size_t idx, const std::function<uchar(size_t)>& value);
        
        //! pass a 1-dim image and set all channels of `channels` to these values
        void set_channels(const uchar* source, const std::set<uint>&channels);
        
        //! access pixel at y,x and channel
        uchar at(uint y, uint x, uint channel = 0) const;
        uchar* ptr(uint y, uint x) const;
        
        void set_pixel(uint x, uint y, const gui::Color& color) const;
        
        void get(cv::Mat& matrix) const;
        cv::Mat get() const;
        bool empty() const { return _data == nullptr; }
        Bounds bounds() const { return Bounds(0, 0, static_cast<Float2_t>(cols), static_cast<Float2_t>(rows)); }
        Size2 dimensions() const { return Size2(static_cast<Float2_t>(cols), static_cast<Float2_t>(rows)); }
        auto channels() const noexcept { return dims; }
        auto stamp() const { return _timestamp; }//return std::chrono::time_point_cast<std::chrono::microseconds>(_timestamp).time_since_epoch().count(); }
        
        bool operator==(const Image& other) const {
            return other.cols == cols && other.rows == rows && other.dims == dims && (_data == other._data || (_size == other._size && memcmp(_data.get(), other._data.get(), _size) == 0));
        }
        
        std::string toStr() const;
        static std::string class_name() {
            return "Image";
        }
        static timestamp_t now() {
            return timestamp_t((timestamp_t::value_type)std::chrono::time_point_cast<std::chrono::microseconds>(clock_::now()).time_since_epoch().count());
        }
        
    private:
        void reset_stamp() {
            _timestamp = now();
        }
    };

    void to_png(const Image& input, std::vector<uchar>& output);
    Image::Ptr from_png(const file::Path& path);
    
    cv::Mat restrict_image_keep_ratio(const Size2& max_size, const cv::Mat& input);
}
