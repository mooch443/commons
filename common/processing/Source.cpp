#include "Source.h"
#include <misc/ThreadPool.h>

namespace cmn::CPULabeling {


void Source::reserve(size_t N) {
    _row_y.reserve(N);
    _row_offsets.reserve(N);
    
    _pixels.reserve(N);
    _ptrs.reserve(N);
    //_full_lines.reserve(N);
    //_nodes.reserve(N);
}

size_t Source::num_rows() const {
    return _row_offsets.size();
}

bool Source::empty() const {
    return _row_offsets.empty();
}

void Source::clear() {
    _pixels.clear();
    _ptrs.clear();
    //_full_lines.clear();
     //_nodes.clear();
    _row_y.clear();
    _row_offsets.clear();
    lw = lh = 0;
}

void Source::push_back(const Line_t& line, const Pixel& px) {
    if(_row_offsets.empty() || line.y() > _row_y.back()) {
        _row_y.push_back(line.y());
        _row_offsets.push_back(_ptrs.size());
    }
    
    _pixels.emplace_back(px);
    _ptrs.push_back({
        line,
        nullptr
    });
    //_lines.emplace_back((const Line*)_full_lines.size());
    //_full_lines.emplace_back(line);
    //_nodes.emplace_back(nullptr);
}

void Source::push_back(const Line_t& line) {
    if(_row_offsets.empty() || line.y() > _row_y.back()) {
        _row_y.push_back(line.y());
        _row_offsets.push_back(_ptrs.size());
    }
    
    assert(_pixels.empty()); // needs to be consistent! dont add pixels "sometimes"
    //_lines.emplace_back(std::move(line));
    //_lines.emplace_back((const Line*)_full_lines.size());
    //_full_lines.emplace_back(line);
    //_nodes.emplace_back(nullptr);
    _ptrs.push_back({
        line,
        nullptr
    });
}

//! assumes external ownership of Line ptr -- needs to stay alive during the process
/*void push_back(const Line* line, const Pixel& px) {
    if(_row_offsets.empty() || line->y > _row_y.back()) {
        _row_y.push_back(line->y);
        _row_offsets.push_back(_lines.size());
    }
    
    _pixels.emplace_back(px);
    _lines.emplace_back(line);
    _nodes.emplace_back(nullptr);
}*/

/**
 * Initialize source entity based on an OpenCV image. All 0 pixels are interpreted as background. This function extracts all horizontal lines from an image and saves them inside, along with information about where which y-coordinate is located.
 */
void Source::init(const cv::Mat& image, bool enable_threads) {
    // assuming the number of threads allowed is < 255
    static GenericThreadPool pool(max(1u, cmn::hardware_concurrency()), "extract_lines");
    
    assert(image.cols < USHRT_MAX && image.rows < USHRT_MAX);
    assert(image.type() == CV_8UC1 || image.type() == CV_8UC3);
    assert(image.isContinuous());
    
    clear();
    
    //! local width, height
    lw = image.cols;
    lh = image.rows;
    
    if(enable_threads
       && int32_t(image.cols)*int32_t(image.rows) > int32_t(100*100))
    {
        /**
         * FIND HORIZONTAL LINES IN ORIGINAL IMAGE
         */
        uint8_t current_index{0};
        std::mutex m;
        std::condition_variable variable;
        
        distribute_indexes([&](const uint8_t, int32_t start, int32_t end, const uint8_t i){
            Source source{
                .lw = lw,
                .lh = lh,
            };
            
            //! perform the actual work
            Source::extract_lines(image, &source, Range<int32_t>(start, end));
            
            // now merge lines (partly) in parallel:
            std::unique_lock guard(m);
            while(current_index < i) {
                variable.wait(guard);
            }
            
            size_t S = _ptrs.size();
            _pixels.insert(_pixels.end(), source._pixels.begin(), source._pixels.end());
            //_full_lines.insert(_full_lines.end(), source._full_lines.begin(), source._full_lines.end());
            /*for(auto &o : source._lines) {
                o = (const HorizontalLine*)((size_t)o + S);
            }*/
            _ptrs.insert(_ptrs.end(), std::make_move_iterator(source._ptrs.begin()), std::make_move_iterator(source._ptrs.end()));
            //_nodes.insert(_nodes.end(), std::make_move_iterator(source._nodes.begin()), std::make_move_iterator(source._nodes.end()));
            
            for(auto &o : source._row_offsets)
                o += S;
                
            _row_offsets.insert(_row_offsets.end(), std::make_move_iterator(source._row_offsets.begin()), std::make_move_iterator(source._row_offsets.end()));
            _row_y.insert(_row_y.end(), std::make_move_iterator(source._row_y.begin()), std::make_move_iterator(source._row_y.end()));
            
            ++current_index;
            variable.notify_all();
            
        }, pool, int32_t(0), int32_t(lh));
        
    } else {
        extract_lines(image, this, Range<int32_t>{0, int32_t(lh)});
    }
    
    finalize();
}

/**
 * Finds all HorizontalLines of POIs within rows range.start to range.end.
 * @param image the source image
 * @param source this is where the converted input data is written to
 * @param rows the y-range for this call
 */
void Source::extract_lines(const cv::Mat& image, Source* source, const Range<int32_t>& rows) {
    const coord_t rstart = rows.start;
    const coord_t rend = rows.end;
    Pixel start, end_ptr;

    static size_t samples = 0, lines = 0;

    static std::shared_mutex mutex;
    bool prev = false;
    Line_t current;
    size_t RESERVE;
    {
        std::shared_lock guard(mutex);
        if (samples == 0)
            RESERVE = (rend - rstart + 1);
        else
            RESERVE = lines / samples;
    }

    source->reserve(RESERVE);

    const int64_t step = image.step.p[0];
    const int64_t step_px = image.step.p[1];
    
    start = image.ptr(rstart);
    end_ptr = image.ptr(rstart + 1);
    
    const ptr_safe_t channels = image.channels();
    assert(end_ptr == start + ptr_safe_t(image.cols) * step_px);
    //end_ptr = start + image.cols;

    for(coord_t i = rstart; i < rend; ++i, start += step, end_ptr += step) {
        //start = image.ptr(i);
        //end_ptr = start + image.cols;
        auto ptr = start;
        
        // walk columns
        for(coord_t col = 0; ptr != end_ptr; ptr += step_px, ++col) {
            if(prev) {
                /// previous is set, now check the current pixel
                /// to see if our hline ended (check all channels):
                /// (we also say it has ended when it is getting too long)
                auto lL = col - 1 - ptr_safe_t(current.x0());
                bool is_px_set = lL >= Line_t::bit_size_x1;
                if(not is_px_set) {
                    for(int64_t p = 0; p < step_px; ++p) {
                        if(*(ptr + p)) {
                            is_px_set = true;
                            break;
                        }
                    }
                }
                
                if(not is_px_set) {
                    /// but current is not set,
                    /// (the last hline just ended)
                    assert(ptr >= start);
                    assert(col >= 1);
                    
                    current.x1(col - 1);
                    source->push_back(current, start + current.x0() * step_px);
                    
                    prev = false;
                }
                
            } else {
                for(int64_t p = 0; p < step_px; ++p) {
                    if(*(ptr + p)) {// !prev && curr (hline starts)
                        current = Line_t(col, col, i);
                        //current.y(i);
                        //current.x0(col);
                        //current.y = i;
                        //current.x0 = col;
                        
                        prev = true;
                        break;
                    }
                }
            }
        }
        
        // if prev is set, the last hline ended when the
        // x-dimension ended, so set x1 accordingly
        if(prev) {
            assert(current.x0() <= source->lw - 1);
            current.x1(source->lw - 1);
            //current.x1 = source->lw - 1;
            source->push_back(current, start + current.x0() * step_px);
            prev = false;
        }
    }

    std::lock_guard guard(mutex);
    ++samples;
    lines += source->_ptrs.size();
    if (samples % 1000 == 0) {
        lines = lines / samples;
        samples = 1;

    }
}

}
