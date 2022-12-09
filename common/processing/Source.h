#pragma once

#include <commons.pc.h>
#include <misc/detail.h>
#include <misc/ranges.h>
#include <processing/Node.h>
#include <processing/HLine.h>

namespace cmn::CPULabeling {

struct Source {
    std::vector<Pixel> _pixels;
    
    struct LinePtr {
        Line line;
        Node::Ptr node;
    };
    
    std::vector<LinePtr> _ptrs;
    //std::vector<Line> _lines;
    //std::vector<Line> _full_lines;
    //std::vector<typename Node::Ptr> _nodes;
    
    std::vector<coord_t> _row_y;
    std::vector<size_t> _row_offsets;
    
    coord_t lw = 0, lh = 0;
    
    void reserve(size_t N);
    size_t num_rows() const;
    bool empty() const;
    
    void clear();
    
    void push_back(const Line& line, const Pixel& px);
    void push_back(const Line& line);
    
    void finalize() {}
    
    class RowRef {
    protected:
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
            //_iterator& operator=(const _iterator& it) = default;
            
            constexpr self_type operator++() {
                assert(ptr_.Lit+1 <= ptr_.obj->line_end);
                ++ptr_.Lit;
                //++ptr_.Nit;
                ++ptr_.Pit;
                return ptr_;
            }
            
            constexpr reference operator*() { assert(ptr_.Lit < ptr_.obj->line_end); return ptr_; }
            constexpr pointer operator->() { assert(ptr_.Lit < ptr_.obj->line_end); return &ptr_; }
            constexpr bool operator==(const self_type& rhs) const { return ptr_ == rhs.ptr_; }
            constexpr bool operator!=(const self_type& rhs) const { return ptr_ != rhs.ptr_; }
        public:
            value_type ptr_;
        };
        
    public:
        struct end_tag {};
        
        Source* _source = nullptr;
        size_t idx;
        coord_t y;
        
        const Pixel* pixel_start = nullptr;
        const Pixel* pixel_end = nullptr;
        //Node::Ptr* node_start = nullptr;
        //Node::Ptr* node_end = nullptr;
        LinePtr*  line_start = nullptr;
         LinePtr*  line_end = nullptr;
        
    public:
        struct Combined {
            const RowRef* obj;
            decltype(RowRef::line_start) Lit;
            //decltype(RowRef::node_start) Nit;
            decltype(RowRef::pixel_start) Pit;
            
            Combined(const RowRef& obj)
                : obj(&obj), Lit(obj.line_start),
                  //Nit(obj.node_start),
                  Pit(obj.pixel_start)
            {}
            
            Combined(const RowRef& obj, end_tag)
                : obj(&obj), Lit(obj.line_end),
                  //Nit(obj.node_end),
                  Pit(obj.pixel_end)
            {}
            
            bool operator!=(const Combined& other) const {
                return Lit != other.Lit; // assume all are the same
            }
            bool operator==(const Combined& other) const {
                return Lit == other.Lit; // assume all are the same
            }
        };
        
        bool valid() const {
            return _source != nullptr && line_start != line_end;
        }
        
        typedef _iterator<Combined> iterator;
        typedef _iterator<const Combined> const_iterator;
        
        iterator begin() { return _iterator<Combined>(Combined(*this)); }
        iterator end() { return _iterator<Combined>(Combined(*this, end_tag{} )); }
        
        void inc_row() {
            if (!valid())
                return;

            pixel_start = pixel_end;
            //node_start = node_end;
            line_start = line_end;
            
            ++idx;
            
            if(idx+1 < _source->_row_y.size()) {
                size_t idx1 = idx+1 >= _source->_row_offsets.size() ? _source->_ptrs.size() : _source->_row_offsets.at(idx+1);
                y = _source->_row_y[idx];
                
                line_end  = _source->_ptrs.data()  + idx1;
                //node_end  = _source->_nodes.data()  + idx1;
                pixel_end = _source->_pixels.data() + idx1;
                
            } else {
                y = std::numeric_limits<coord_t>::max();
            }
        }
        
        /**
         * Constructs a RowRef with a given y-coordinate (or bigger).
         * @param source Source object that contains the desired row
         * @param y y-coordinate of the desired row
         */
        static RowRef from_index(Source* source, coord_t y) {
            auto it = std::upper_bound(source->_row_y.begin(), source->_row_y.end(), y);
            
            // beyond the value ranges
            if(it == source->_row_y.end()) {
                return RowRef(); // return nullptr
            }
            
            // if the found row is bigger than the desired y, then it will either be because the y we sought is the previous element, or because it does not exist.
            if(it > source->_row_y.begin() && *(it - 1)  == y) {
                --it;
            }
            
            y = *it;
            
            size_t idx = std::distance(source->_row_y.begin(), it);
            size_t idx0 = source->_row_offsets.at(idx);
            size_t idx1 = idx0+1 == source->_row_offsets.size() ? source->_ptrs.size() : source->_row_offsets.at(idx+1);
            
            return RowRef{
                source,
                
                idx,
                y,
                
                source->_pixels.data() + idx0,
                source->_pixels.data() + idx1,
                //source->_nodes.data() + idx0,
                //source->_nodes.data() + idx1,
                source->_ptrs.data() + idx0,
                source->_ptrs.data() + idx1
            };
        }
    };
    
    template<typename F, typename Index, typename Pool>
        requires std::integral<Index>
    void distribute_indexes(F&& fn, Pool& pool, Index start, Index end) {
        const auto threads = Index(min(254u, pool.num_threads()));
        Index i = 0, N = end - start;
        uint8_t thread_index = 0;
        const Index per_thread = max(Index(1), N / threads);
        Index processed = 0, enqueued = 0;
        std::mutex mutex;
        std::condition_variable variable;
        
        Index nex = start;
        
        for(auto it = start; it != end;) {
            auto step = i + per_thread < N ? per_thread : (N - i);
            nex += step;
            
            assert(step > 0);
            if(nex == end) {
                fn(thread_index, it, nex, step);
                
            } else {
                ++enqueued;
                
                pool.enqueue([&](uint8_t thread_index, auto it, auto nex, auto step) {
                    fn(thread_index, it, nex, step);
                    
                    std::unique_lock g(mutex);
                    ++processed;
                    variable.notify_one();
                    
                }, thread_index, it, nex, step);
            }
            
            it = nex;
            i += step;
            ++thread_index;
        }
        
        std::unique_lock g(mutex);
        while(processed < enqueued)
            variable.wait(g);
    }
    
    /**
     * Initialize source entity based on an OpenCV image. All 0 pixels are interpreted as background. This function extracts all horizontal lines from an image and saves them inside, along with information about where which y-coordinate is located.
     */
    void init(const cv::Mat& image, bool enable_threads);
    
    /**
     * Constructs a RowRef struct for a given y-coordinate (see RowRef::from_index).
     */
    RowRef row(const coord_t y) {
        return RowRef::from_index(this, y);
    }
    
    /**
     * Finds all HorizontalLines of POIs within rows range.start to range.end.
     * @param image the source image
     * @param source this is where the converted input data is written to
     * @param rows the y-range for this call
     */
    static void extract_lines(const cv::Mat& image, Source* source, const Range<int32_t>& rows);
};

}
