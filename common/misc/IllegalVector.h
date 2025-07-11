#pragma once
#include <commons.pc.h>

namespace cmn {

template<typename T>
//requires (std::is_trivial_v<T>)
class IllegalArray {
    T* _ptr{nullptr};
    std::size_t _capacity{0u};
    std::size_t _size{0u};
public:
    using value_type = T;
    
public:
    IllegalArray() noexcept = default;
    IllegalArray(const IllegalArray& other) noexcept {
        resize(other._size);
        std::memcpy(_ptr, other._ptr, other._size * sizeof(T));
    }
    IllegalArray(IllegalArray&& other) noexcept
        : _ptr(other._ptr),
          _capacity(other._capacity),
          _size(other._size)
    {
        other._ptr = nullptr;
        other._capacity = 0;
        other._size = 0;
    }
    
    IllegalArray& operator=(const IllegalArray& other) noexcept {
        if(this != &other) {
            resize(other._size);
            std::memcpy(_ptr, other.data(), other._size * sizeof(T));
        }
        return *this;
    }
    
    IllegalArray& operator=(IllegalArray&& other) noexcept {
        if(this != &other) {
            this->~IllegalArray();
            new (this) IllegalArray(std::move(other));
        }
        return *this;
    }
    
    IllegalArray& operator=(std::initializer_list<T> objects) noexcept {
        const std::size_t N = objects.size();
        resize(N);
        std::copy(objects.begin(), objects.end(), _ptr);
        return *this;
    }
    
    explicit IllegalArray(const T* start, const T* end) noexcept {
        assert(end >= start);
        std::size_t N = end - start;
        resize(N);
        std::copy_n(start, N, _ptr);
    }
    
    explicit IllegalArray(std::size_t N) noexcept {
        resize(N);
    }
    
    IllegalArray(std::initializer_list<T> objects) noexcept
        : IllegalArray(objects.begin(), objects.end())
    { }
    
    void reserve(std::size_t N) noexcept {
        if(N > _size) {
            if(_ptr) {
                if(_capacity < N) {
                    _capacity = N * 2u;
                    _ptr = (T*)std::realloc(_ptr, _capacity * sizeof(T));
                }
            } else {
                _capacity = N + N / 2u;
                _ptr = (T*)std::malloc(_capacity * sizeof(T));
            }
        }
    }
    
    void resize(std::size_t N) noexcept {
        reserve(N);
        _size = N;
        assert(_size <= _capacity);
    }
    
    void resize(std::size_t N, T obj) noexcept {
        resize(N);
        std::fill(begin(), end(), obj);
    }
    
    T& operator[](std::size_t index) noexcept {
        assert(index < _size);
        return _ptr[index];
    }
    T at(std::size_t index) const noexcept {
        assert(index < _size);
        return _ptr[index];
    }
    T operator[](std::size_t index) const noexcept {
        assert(index < _size);
        return _ptr[index];
    }
    
    template<typename... K>
    void push_back(K&& ... obj) noexcept {
        static constexpr std::size_t N = sizeof...(K);
        std::size_t i = _size;
        resize(i + N);
        
        ((_ptr[i++] = std::forward<K>(obj)), ...);
    }
    
    void assign(const T* start, const T* end) {
        assert(end >= start);
        std::size_t N = end - start;
        if(N == 0)
            return;
        resize(N);
        std::copy_n(start, N, _ptr);
    }
    
    void insert(T* it, T obj, std::size_t N) noexcept {
        if(empty()) {
            // we are empty!
            assert(it == nullptr);
            resize(N, obj);
            return;
        }
        
        std::size_t end = _size;
        assert(it >= _ptr && it <= _ptr + end);
        std::size_t index = it - _ptr;
        
        resize(_size + N);
        
        if(index < end)
            std::memmove(_ptr + index + N, _ptr + index, (end - index) * sizeof(T));
        
        //if constexpr(std::integral<T>)
        //    std::memset(_ptr + index, obj, N);
        //else {
            std::fill(_ptr + index, _ptr + index + N, obj);
        //}
    }
    
    T* insert(T* it, const T* start, const T* end) noexcept {
        if(not _ptr) {
            // we are empty!
            assert(it == nullptr);
            
            this->~IllegalArray();
            new (this) IllegalArray(start, end);
            return _ptr;
        }
        
        assert(it >= _ptr && it <= _ptr + _size);
        assert(end >= start);
        
        const std::size_t last_index = _size;
        std::size_t index = it - _ptr;
        std::size_t N = end - start;
        
        resize(_size + N);
        
        if(index < last_index)
            std::memmove(_ptr + index + N, _ptr + index, (last_index - index) * sizeof(T));
        
        std::copy(start, end, _ptr + index);
        return _ptr + index;
    }
    
    const T* data() const noexcept {
        assert(_ptr != nullptr);
        return _ptr;
    }
    T* data() noexcept {
        assert(_ptr != nullptr);
        return _ptr;
    }
    
    T back() const {
        assert(_size > 0 && _ptr);
        return *(_ptr + (_size - 1u));
    }
    
    T front() const {
        assert(_size > 0 && _ptr);
        return *_ptr;
    }
    
    std::size_t size() const noexcept {
        return _size;
    }
    
    std::size_t capacity() const noexcept {
        return _capacity;
    }
    
    bool empty() const noexcept {
        return not _ptr || _size == 0;
    }
    
    void clear() noexcept {
        _size = 0;
    }
    
    // ------------------------------------------------------------
    // Comparison operators
    // ------------------------------------------------------------
    bool operator==(const IllegalArray& other) const noexcept
    {
        if (this == &other)              return true;          // same object
        if (_size != other._size)       return false;         // different sizes
        return std::equal(_ptr, _ptr + _size, other._ptr);    // element-wise compare
    }

    bool operator!=(const IllegalArray& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(const IllegalArray& other) const noexcept
    {
        return std::lexicographical_compare(_ptr, _ptr + _size,
                                            other._ptr, other._ptr + other._size);
    }

    bool operator>(const IllegalArray& other) const noexcept
    {
        return other < *this;
    }

    bool operator<=(const IllegalArray& other) const noexcept
    {
        return !(other < *this);
    }

    bool operator>=(const IllegalArray& other) const noexcept
    {
        return !(*this < other);
    }
    
    // ---------------------------------------------------------------------
    // Pointer‑based iterator support so the container works with range loops
    // ---------------------------------------------------------------------
    using iterator               = T*;
    using const_iterator         = const T*;
    
    iterator begin()             { return _ptr; }
    iterator end()               { return _ptr + _size; }
    
    const_iterator begin() const { return _ptr; }
    const_iterator end()   const { return _ptr + _size; }
    
    const_iterator cbegin() const { return _ptr; }
    const_iterator cend()   const { return _ptr + _size; }
    
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    reverse_iterator       rbegin()        noexcept { return reverse_iterator(end()); }
    reverse_iterator       rend()          noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(begin()); }

    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crend()   const noexcept { return const_reverse_iterator(begin()); }
    
    ~IllegalArray() {
        if(_ptr)
            free(_ptr);
    }
};

}
