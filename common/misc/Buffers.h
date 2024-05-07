#pragma once
#include <commons.pc.h>
//#undef NDEBUG
//#define DEBUG_BUFFER_IMAGES 1

namespace cmn {

template <typename Func, typename Arg>
concept CallableWith = std::is_invocable_v<Func, Arg>;

template<typename T, size_t _capacity>
class FixedCapacityVector {
    size_t _size{0u};
    std::array<T, _capacity> _data;

public:
    static constexpr size_t capacity() {
        return _capacity;
    }
    
    size_t size() const {
        return _size;
    }
    
    bool empty() const {
        return _size == 0u;
    }
    
    void push_back(T&& item) {
        if(_size >= _capacity)
            throw U_EXCEPTION("Buffer is full");
        
        _data[_size++] = std::move(item);
    }

    void push_back(const T& item) {
        if(_size >= _capacity)
            throw U_EXCEPTION("Buffer is full");
        
        _data[_size++] = item;
    }

    T& operator[](size_t index) {
        if(index >= _size)
            throw U_EXCEPTION("Index out of bounds");
        
        return _data[index];
    }

    const T& operator[](size_t index) const {
        if(index >= _size)
            throw U_EXCEPTION("Index out of bounds");
        
        return _data[index];
    }

    T& back() {
        if(_size == 0u)
            throw U_EXCEPTION("Buffer is empty");
        
        return _data[_size - 1u];
    }

    const T& back() const {
        if(_size == 0u)
            throw U_EXCEPTION("Buffer is empty");
        
        return _data[_size - 1u];
    }

    T& front() {
        if(_size == 0u)
            throw U_EXCEPTION("Buffer is empty");
        
        return _data[0];
    }

    const T& front() const {
        if(_size == 0u)
            throw U_EXCEPTION("Buffer is empty");
        
        return _data[0];
    }

    void pop_back() {
        if(_size == 0u)
            throw U_EXCEPTION("Buffer is empty");
        
        --_size;
    }

    void clear() {
        _size = 0u;
    }

    T* data() {
        return _data.data();
    }

    const T* data() const {
        return _data.data();
    }

    T* begin() {
        return _data.data();
    }

    const T* begin() const {
        return _data.data();
    }

    T* end() {
        return _data.data() + _size;
    }

    const T* end() const {
        return _data.data() + _size;
    }
};

template<typename T, typename Construct, size_t _max_size = 0>
class Buffers {
public:
    using buffer_t = typename std::conditional<_max_size != 0, FixedCapacityVector<T, _max_size>, std::vector<T>>::type;
    
private:
    buffer_t _buffers;
    Construct _create{};
    mutable std::mutex m;
    
public:
    std::mutex& mutex() const {
        return m;
    }
    
    static constexpr size_t max_size() {
        return _max_size;
    }
    
    size_t size() const {
        std::unique_lock guard(mutex());
        return _buffers.size();
    }
    
    T get(cmn::source_location loc) {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
#ifdef DEBUG_BUFFER_IMAGES
            if(not ptr)
                throw U_EXCEPTION("Failed to get buffer");
#endif

            _buffers.pop_back();
            return ptr;
        }
        
        if constexpr(std::is_invocable_v<Construct, cmn::source_location&&>)
            return _create(std::move(loc));
        else
            return _create();
    }
    
    void move_back(T&& image) {
        std::unique_lock guard(mutex());
        if(image) {
            if(_max_size > 0u
               && _buffers.size() + 1u >= _max_size)
            {
                return; // delete item since we are beyond max
            }
                
            _buffers.push_back(std::move(image));
        }
    }
};

struct ImageSource : public Image::CustomData {
public:
    const char* _origin;
    ImageSource(const char* origin) : _origin(origin) {}
};

template<typename T, typename Construct>
class ImageBuffers {
    std::mutex& mutex() {
        return m;
    }
    std::vector<T> _buffers;
    Construct _create{};
    std::mutex m;
    Size2 _image_size;
#ifdef DEBUG_BUFFER_IMAGES
    std::string _name;
    std::unordered_set<typename T::element_type*> _created;
#endif

public:
    ImageBuffers(const char* name) 
#ifdef DEBUG_BUFFER_IMAGES
        : _name(name+hex((uint64_t)this).toStr()) 
#endif
    {
        UNUSED(name);
    }
    ImageBuffers(const char* name, Size2 size) 
        : _image_size(size)
#ifdef DEBUG_BUFFER_IMAGES
        , _name(name + hex((uint64_t)this).toStr())
#endif
    {
        UNUSED(name);
    }
    
    T get(cmn::source_location loc) {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
#ifdef DEBUG_BUFFER_IMAGES
            if(not ptr)
                throw U_EXCEPTION("Failed to get buffer");
#endif

            _buffers.pop_back();
            return ptr;
        }
        
        T ptr;
        if constexpr(std::is_invocable_v<Construct, cmn::source_location&&>)
            ptr = _create(std::move(loc));
        else
            ptr = _create();
#ifdef DEBUG_BUFFER_IMAGES
        if (ptr) {
            std::unique_lock guard(mutex());
            _created.insert(ptr.get());

            if constexpr (std::is_same_v<typename T::element_type, Image>)
                ptr->set_custom_data(new ImageSource{ _name.c_str()});

            if (_created.size() > 100)
                thread_print("[", type_name<ImageBuffers>(), "] ", _name, " created ", _created.size(), " images");
        }
#endif
        return ptr;
    }
    
    void move_back(T&& image) {
        std::unique_lock guard(mutex());
        if(image) {
#ifdef DEBUG_BUFFER_IMAGES
            //if (_created.contains(image.get()))
            //    _created.erase(image.get());

            if constexpr (std::same_as<typename T::element_type, Image>) {
                if (const ImageSource* ptr = dynamic_cast<const ImageSource*>(image->custom_data());
                    ptr != nullptr) 
                {
                    if (ptr->_origin != _name) {
                        thread_print("Origin of ", *image, " is not ", _name, " but ", ptr->_origin);
                    }
                }
            }

            if(not _image_size.empty()) {
                if(image->cols != _image_size.width
                    || image->rows != _image_size.height)
                {
                    throw U_EXCEPTION("[",type_name<ImageBuffers>(),"] Invalid image size: ", 
                        image->cols, "x", image->rows, 
                        " != ", 
                        _image_size.width, "x", _image_size.height);
                }
            }
#endif

            _buffers.emplace_back(std::move(image));
        }
    }

    void set_image_size(Size2 size) {
        std::unique_lock guard(mutex());
        _image_size = size;
        if(not _buffers.empty()) {
#ifndef NDEBUG
            FormatWarning("There were already ", _buffers.size(), " images in queue - removing.");
#endif
            _buffers.clear();
        }
    }
};

}
