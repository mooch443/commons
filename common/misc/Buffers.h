#pragma once
#include <commons.pc.h>
//#undef NDEBUG

namespace cmn {

template <typename Func, typename Arg>
concept CallableWith = std::is_invocable_v<Func, Arg>;

template<typename T, typename Construct>
struct Buffers {
    std::mutex& mutex() {
        return m;
    }
    std::vector<T> _buffers;
    Construct _create{};
    std::mutex m;
    
    T get(cmn::source_location loc) {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
#ifndef NDEBUG
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
        if(image)
            _buffers.emplace_back(std::move(image));
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
#ifndef NDEBUG
    const char* _name;
    std::unordered_set<typename T::element_type*> _created;
#endif

public:
    ImageBuffers(const char* name) 
#ifndef NDEBUG
        : _name(name) 
#endif
    {
        UNUSED(name);
    }
    ImageBuffers(const char* name, Size2 size) 
        : _image_size(size)
#ifndef NDEBUG
        , _name(name)
#endif
    {
        UNUSED(name);
    }
    
    T get(cmn::source_location loc) {
        if(std::unique_lock guard(mutex());
           not _buffers.empty())
        {
            auto ptr = std::move(_buffers.back());
#ifndef NDEBUG
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
#ifndef NDEBUG
        if (ptr) {
            std::unique_lock guard(mutex());
            _created.insert(ptr.get());

            if constexpr (std::is_same_v<typename T::element_type, Image>)
                ptr->set_custom_data(new ImageSource{ _name });

            if (_created.size() > 100)
                thread_print("[", type_name<ImageBuffers>(), "] ", _name, " created ", _created.size(), " images");
        }
#endif
        return ptr;
    }
    
    void move_back(T&& image) {
        std::unique_lock guard(mutex());
        if(image) {
#ifndef NDEBUG
            if (_created.contains(image.get()))
                _created.erase(image.get());

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
    }
};

}
