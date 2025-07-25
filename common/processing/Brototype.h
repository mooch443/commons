#pragma once

#include <commons.pc.h>
#include <processing/HLine.h>

namespace cmn::CPULabeling {

class DLList;

inline void _emplace_back(auto& array, auto&& obj) {
    const auto N = array.size();
    //Print("capacity = ", N, " vs ", array.capacity());
    if(array.capacity() <= N) {
        array.reserve(max(64u, N * 2));
        //Print("Had to reserve ", N, " -> ", array.capacity());
    }
        
    array.push_back(std::move(obj));
}

//! A pair of a blob and a HorizontalLine
class Brototype {
private:
    using PVector = IllegalArray<Pixel>;
    using LVector = IllegalArray<Line_t>;
    
    GETTER_NCONST(PVector, pixel_starts);
    GETTER_NCONST(LVector, lines);
    std::vector<Brototype*> _children;
    Brototype* _parent{nullptr};
    
public:
    static std::unordered_set<Brototype*> brototypes();
    constexpr Brototype* has_parent() const {
        return _parent;
    }
    void set_parent(Brototype*);
    void finalize();
    
    Brototype(size_t reserve_hint);
    Brototype(const Line_t& line, const uchar* px);
    ~Brototype();
    
    static std::mutex& mutex();
    static void move_to_cache(DLList *list, typename std::unique_ptr<Brototype>& node);
    
    inline bool empty() const {
        return _lines.empty();
    }
    
    inline size_t size() const {
        return _lines.size();
    }
    
    inline void push_back(const Line_t& line, const uchar* px) {
        _emplace_back(_lines, line);
        _emplace_back(_pixel_starts, px);
    }
    
    void merge_with(const Brototype& b);
    void _merge_with(const Brototype& b);
    void __merge_with(const Brototype& b);
    
    struct Combined {
        decltype(Brototype::_lines)::iterator Lit;
        decltype(Brototype::_pixel_starts)::iterator Pit;
        
        Combined(Brototype& obj)
            : Lit(obj.lines().begin()),
              Pit(obj._pixel_starts.begin())
        {}
        Combined(Brototype& obj, size_t)
            : Lit(obj.lines().end()),
              Pit(obj._pixel_starts.end())
        { }
        
        bool operator!=(const Combined& other) const {
            return Lit != other.Lit; // assume all are the same
        }
        bool operator==(const Combined& other) const {
            return Lit == other.Lit; // assume all are the same
        }
    };
    
    template<typename ValueType>
    class _iterator {
    public:
        typedef _iterator self_type;
        typedef ValueType value_type;
        typedef ValueType& reference;
        typedef ValueType* pointer;
        typedef std::forward_iterator_tag iterator_category;
        typedef int difference_type;
        constexpr _iterator(const value_type& ptr) : ptr_(ptr) { }
        _iterator& operator=(const _iterator& it) = default;
        
        constexpr self_type operator++() {
            ++ptr_.Lit;
            ++ptr_.Pit;
            return ptr_;
        }
        //constexpr self_type operator++(int) { value ++; return *this; }
        constexpr reference operator*() { return ptr_; }
        constexpr pointer operator->() { return &ptr_; }
        constexpr bool operator==(const self_type& rhs) const { return ptr_ == rhs.ptr_; }
        constexpr bool operator!=(const self_type& rhs) const { return ptr_ != rhs.ptr_; }
    public:
        value_type ptr_;
    };
    
    typedef _iterator<Combined> iterator;
    typedef _iterator<const Combined> const_iterator;
    
    iterator begin() { return _iterator<Combined>(Combined(*this)); }
    iterator end() { return _iterator<Combined>(Combined(*this, size())); }
};


}
