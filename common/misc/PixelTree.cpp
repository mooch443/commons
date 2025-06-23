#include "PixelTree.h"

#include <misc/Timer.h>
#include <processing/CPULabeling.h>
#include <processing/LuminanceGrid.h>
#include <misc/ranges.h>
#include <misc/Image.h>
#include <misc/PVBlob.h>

//#define DEBUG_TREE_WALK
//#define DEBUG_EDGES

namespace cmn::pixel {
    static Image::Ptr debug_greyscale = nullptr;

bool Tree::Comparator::operator()(uint64_t left, const Ptr& right) const
{
    return left < right->index;
}

bool Tree::Comparator::operator()(const Ptr & left, uint64_t right) const
{
    return left->index < right;
}

bool Tree::Comparator::operator()(const Ptr& left, const Ptr& right) const
{
    return left->index < right->index;
}

struct Row {
    std::vector<int> cache;
#if TREE_WITH_PIXELS
    PixelArray_t pixels;
#endif
    std::vector<int> border;
    
    Range<int> range;
    int y;
    
    Row(int cols) : range(-1, -1), y(-1) {
        resize(cols);
    }
    
    void resize(size_t x) {
        cache.resize(x);
#if TREE_WITH_PIXELS
        pixels.resize(x);
#endif
        
        std::fill(cache.begin(), cache.end(), -1);
        border.reserve(x);
    }
    
    inline int& operator[](size_t x) {
        return cache[x];
    }
    
    inline int operator[](size_t x) const {
        return cache[x];
    }
    
#if TREE_WITH_PIXELS
    inline int pixel(size_t x) const {
        return pixels[x];
    }
    
    inline void set_pixel(size_t x, int value) {
        pixels[x] = value;
    }
#endif
    
    size_t size() const {
        return cache.size();
    }
    
    void add_border(int index) {
        if(contains(border, index))
            return;
        border.push_back(index);
    }
    
    inline bool valid() const {
        return y != -1;
    }
};



inline blobs_t _threshold_blob(CPULabeling::ListCache_t& cache, pv::BlobWeakPtr blob, const PixelArray_t& difference_cache, int threshold, const Background& background) {
    //Print("* testing threshold ", threshold, " for pixel ", difference_cache.front());
    if(blob->is_binary()) {
        std::vector<blob::Pair> blobs;
        blobs.emplace_back(blob::Pair(std::make_unique<blob::lines_t>(blob->hor_lines()), nullptr, blob->flags(), blob::Prediction(blob->prediction())));
        return blobs;
        
    } else {
        const uint8_t channels = blob->channels();
        
        std::vector<HorizontalLine> lines;
        lines.reserve(blob->hor_lines().size());
        
        const uchar* pxs = blob->pixels()->data();
        const uchar* dpx = difference_cache.data();
		const uchar* const diff_cache_start = difference_cache.data();
        
        uchar* pixels = (uchar*)std::malloc(blob->pixels()->size() * sizeof(uchar));
        //PixelArray_t pixels;
        size_t counted_pixels = 0;
        //pixels.resize(blob->pixels()->size());
        
        for(const auto &line : blob->hor_lines()) {
            const uchar* start{nullptr};
			const size_t L = ptr_safe_t(line.x1) - ptr_safe_t(line.x0) + 1;

            const uchar* dpx_start = dpx;
			const uchar* dpx_end = dpx + L;
            
            //for (auto x=line.x0; x<=line.x1; ++x, px += channels, ++dpx) {
			for (; dpx < dpx_end; ++dpx) {
                
                /*if (not background.is_value_different<OutputInfo{
                    .channels = 1u,
                    .encoding = meta_encoding_t::gray
                }>(x, line.y, *dpx, threshold)) */
                if(*dpx < threshold) {
                    if(start) {
                        const ptr_safe_t L = dpx - start;
                        assert(L > 0);

                        const ptr_safe_t x0 = line.x0 + (start - dpx_start);
                        const ptr_safe_t x1 = x0 + L - 1;

                        const ptr_safe_t px_offset = start - diff_cache_start;
						const uchar* px = pxs + px_offset * channels;

						std::memcpy(pixels + counted_pixels, px, L * channels * sizeof(uchar));
                        counted_pixels += L * channels;
                        
						lines.push_back(HorizontalLine( line.y, x0, x1 ));
                        //Print("Adding line ", line);
                        //pixels.insert(pixels.end(), start, px);
                        //lines.emplace_back(line.y, x0, x - 1);
                        start = nullptr;
                    }
                    
                } else if(!start) {
                    start = dpx;
                }
            }
            
            if(start) {
                const ptr_safe_t L = dpx - start;
                assert(L > 0);
                
                const ptr_safe_t x0 = line.x0 + (start - dpx_start);
                const ptr_safe_t x1 = x0 + L - 1;

                const ptr_safe_t px_offset = start - diff_cache_start;
                const uchar* px = pxs + px_offset * channels;
                assert(x1 == line.x1);

                //const size_t N = pixels.size();
                //pixels.resize(N + L * channels);
                std::memcpy(pixels + counted_pixels, px, L * channels * sizeof(uchar));
                counted_pixels += L * channels;

                lines.push_back(HorizontalLine(line.y, x0, line.x1));
            }
        }
        
        auto blobs = CPULabeling::run(lines, std::span<uchar>(pixels, pixels + counted_pixels), cache, channels);
        for(auto &pair : blobs) {
            pair.extra_flags |= pv::Blob::copy_flags(*blob);
            pair.pred = blob->prediction();
            //assert(pv::Blob::is_flag(pair.extra_flags, pv::Blob::Flags::is_r3g3b2) == blob->is_r3g3b2());
        }
        return blobs;
    }
}


    inline blobs_t _threshold_blob(CPULabeling::ListCache_t& cache,
                                   pv::BlobWeakPtr blob,
                                   int threshold,
                                   const Background* bg,
                                   uint8_t /*use_closing*/ = 0,
                                   uint8_t /*closing_size*/ = 2)
#ifdef NDEBUG
        noexcept
#endif
    {
        if(blob->is_binary()) {
            /// TODO
            auto lines = std::make_unique<blob::line_ptr_t::element_type>(*blob->lines());
            auto pair = blob::Pair(std::move(lines), nullptr, blob->flags(), blob::Prediction(blob->prediction()));
            std::vector<blob::Pair> vector;
            vector.emplace_back(std::move(pair));
            return vector;
        }
        
#ifndef NDEBUG
        if(!blob->pixels())
            throw U_EXCEPTION("Cannot threshold a blob without pixels.");
#endif
        //return blob;
        //blob->threshold(threshold, *bg);
        //Timer timer;
        /*if(use_closing || blob->pixels()->size() > 100 * 10) {
            static cv::Mat local;
            auto [pos, image] = blob->gray_image(NULL, Bounds(0, 0, bg->image().cols, bg->image().rows));
            
            {
                static gpuMat back;
                static gpuMat element;
                static std::mutex element_mutex;
                
                {
                    std::lock_guard<std::mutex> guard(element_mutex);
                    if(element.empty() || element.cols != closing_size * 2 + 1) {
                        cv::Mat mat = cv::getStructuringElement(cv::MORPH_ELLIPSE, (cv::Size)Size2(closing_size * 2 + 1));
                        mat.copyTo(element);
                    }
                    
                    if(back.empty()) {
                        bg->image().get().copyTo(back);
                    }
                }
                
                gpuMat mat, buffer1;
                image->get().copyTo(mat);
                
                auto bds = Bounds(pos, image->bounds().size());
                bds.restrict_to(Bounds(0, 0, back.cols, back.rows));
                cv::subtract(back(bds), mat, buffer1);
                
                for(uint8_t i=0; i<use_closing; ++i) {
                    cv::erode(buffer1, buffer1, element);
                    cv::dilate(buffer1, buffer1, element);
                }
                
                cv::threshold(buffer1, buffer1, threshold, 255, cv::THRESH_BINARY);
                
                mat.copyTo(local, buffer1);
            }
            
            auto blobs = CPULabeling::run(local, cache);
            
            for(auto && [lines, pixels, flags, pred] : blobs) {
                for(auto &line : *lines) {
                    line.y += pos.y;
                    line.x0 += pos.x;
                    line.x1 += pos.x;
                }
                pred = blob->prediction();
            }
            
            return blobs;
        }*/
        
        //timer.reset();
        auto px = blob->pixels()->data();
        std::vector<HorizontalLine> lines;
        std::vector<uchar, NoInitializeAllocator<uchar>> pixels(NoInitializeAllocator<uchar>{});
        lines.reserve(blob->hor_lines().size());
        pixels.reserve(blob->pixels()->size());
        
        //const ImageMode mode = blob->is_rgb()
        //    ? ImageMode::RGB
        //    : (blob->is_r3g3b2() ? ImageMode::R3G3B2 : ImageMode::GRAY);
        const InputInfo input = blob->input_info();
        OutputInfo output;
        output = input;
        
        if(bg)
            call_image_mode_function(input, output, [&]<InputInfo i, OutputInfo o, DifferenceMethod method>(){
                line_without_grid<i, o, method>(bg, blob->hor_lines(), px, threshold, lines, pixels);
            });
        else
            call_image_mode_function(input, output, [&]<InputInfo i, OutputInfo o, DifferenceMethod method>(){
                line_without_bg<i, o, method>(bg, blob->hor_lines(), px, threshold, lines, pixels);
            });
        
        auto blobs = CPULabeling::run(lines, pixels, cache, blob->channels());
        for(auto &pair : blobs) {
            pair.extra_flags |= pv::Blob::copy_flags(*blob);
            //assert(pv::Blob::is_flag(pair.extra_flags, pv::Blob::Flags::is_r3g3b2) == blob->is_r3g3b2());
            pair.pred = blob->prediction();
        }
        return blobs;
    }
    
    pv::BlobPtr threshold_get_biggest_blob(pv::BlobWeakPtr blob, int threshold, const Background* bg, uint8_t use_closing, uint8_t closing_size, CPULabeling::ListCache_t&& cache) {
        auto blobs = _threshold_blob(cache, blob, threshold, bg, use_closing, closing_size);
        
        size_t max_size = 0;
        blobs_t::value_type *found = nullptr;
        for(auto &tup : blobs) {
            auto && [lines, pixels, flags, pred] = tup;
            assert(pixels || blob->is_binary());
            if(pixels) {
                if(pixels->size() > max_size) {
                    found = &tup;
                    max_size = pixels->size();
                }
            } else {
                pv::Blob tmp(std::move(lines), nullptr, flags | pv::Blob::copy_flags(*blob), blob::Prediction(pred));
                if(tmp.num_pixels() > max_size) {
                    found = &tup;
                    max_size = tmp.num_pixels();
                }
                
                lines = std::move(tmp.steal_lines());
            }
        }
        
        if(found) {
            auto r = pv::Blob::Make(
                              std::move(found->lines),
                              std::move(found->pixels),
                              found->extra_flags,
                              blob::Prediction{blob->prediction()});
            assert(r->is_rgb() == blob->is_rgb());
            assert(r->is_r3g3b2() == blob->is_r3g3b2());
            assert(r->is_binary() == blob->is_binary());
            return r;
        }
        
        uint8_t extra_flags = 0;
        extra_flags |= pv::Blob::copy_flags(*blob);
        return pv::Blob::Make(
                std::make_unique<std::vector<HorizontalLine>>(),
                std::make_unique<PixelArray_t>(),
                extra_flags,
                blob::Prediction{blob->prediction()});
        //auto ptr = pv::Blob::Make(lines, pixels);
        //return ptr;
    }
    
    std::vector<pv::BlobPtr> threshold_blob(CPULabeling::ListCache_t& cache, pv::BlobWeakPtr blob, int threshold, const Background* bg, const Rangel& size_range) {
        std::vector<pv::BlobPtr> result;
        if(blob->is_binary()) {
            result.emplace_back(pv::Blob::Make(*blob));
        } else {
            auto blobs = _threshold_blob(cache, blob, threshold, bg);
            for(auto&& [lines, pixels, flags, pred] : blobs) {
                if((size_range.end < 0 && pixels->size() > 1) || ((long_t)pixels->size() > size_range.start && (long_t)pixels->size() < size_range.end))
                    result.emplace_back(pv::Blob::Make(std::move(lines), std::move(pixels), flags, std::move(pred)));
            }
        }
        return result;
    }

    /*std::vector<pv::BlobPtr> threshold_blob(pv::BlobWeakPtr blob, int threshold, const Background* bg, const Rangel& size_range) {
        CPULabeling::ListCache_t cache;
        return threshold_blob(cache, blob, threshold, bg, size_range);
    }*/

std::vector<pv::BlobPtr> threshold_blob(CPULabeling::ListCache_t& cache, pv::BlobWeakPtr blob, const PixelArray_t& difference_cache, int threshold, const Background& background, const Rangel& size_range) {
    std::vector<pv::BlobPtr> result;
    if(blob->is_binary()) {
        result.emplace_back(pv::Blob::Make(*blob));
    } else {
        auto blobs = _threshold_blob(cache, blob, difference_cache, threshold, background);
        for(auto && [lines, pixels, flags, pred] : blobs) {
            if((size_range.end < 0 && pixels->size() > 1) || ((long_t)pixels->size() > size_range.start && (long_t)pixels->size() < size_range.end))
                result.emplace_back(pv::Blob::Make(std::move(lines), std::move(pixels), flags, std::move(pred)));
        }
    }
    return result;
}

    /*std::vector<pv::BlobPtr> threshold_blob(pv::BlobWeakPtr blob, const PixelArray_t& difference_cache, int threshold, const Rangel& size_range) {
        CPULabeling::ListCache_t cache;
        return threshold_blob(cache, blob, difference_cache, threshold, size_range);
    }*/
    
    
    
    inline void finalize(int offx, Tree& tree, std::shared_ptr<Row>& previous_row, std::shared_ptr<Row>& current_row, std::shared_ptr<Row>& next_row, int y, int x0, int x1)
    {
        if(!next_row->valid()) {
            if(next_row->range.start >= 0)
                throw U_EXCEPTION("Both");
        }
        else if(y > next_row->y+1) {
            //! ---- CASE 0 ----
            /// rows are more than 1 pixel apart, so everything in next_row is a border
            /// or there is no next row, in which case its also border
            if(next_row->range.start >= 0) {
                for(int x=next_row->range.start; x<=next_row->range.end; ++x) {
                    if((*next_row)[x] == next_row->y)
                        next_row->add_border(x);
                }
            }
            
        } else {
            //! ---- CASE 1 ----
            /// consecutive rows
            
            // the newly started row begins after the last read row begins (valid)
            if(x0 > next_row->range.start) {
                // set all pixels to border that are within last rows range
                // and end before the next row
                for (int i=next_row->range.start; i<x0 && i<=next_row->range.end; ++i) {
                    if((*next_row)[i] == next_row->y)
                        next_row->add_border(i);
                }
            }
            
            // the newly started row ends before the previous row ends, so they either overlap
            // or they follow each other. iterate from the right-most overlap point to the end
            // of the previous row and set it to border.
            // (everything before the start of the next_row does not matter, cause it will be set
            // using prev_x later on)
            if(next_row->range.end < current_row->range.end) {
                for (int i=max(current_row->range.start, next_row->range.end+1); i<=current_row->range.end; ++i) {
                    if((*current_row)[i] == next_row->y-1)
                        current_row->add_border(i); /* next_row->y - 1 ? */
                }
            }
        }
        
        // IF CURRENT ROW IS VALID:
        // we definitely finished current_row (now that its being moved into prev_row)
        // so we can determine / add all the border points
        if(current_row->y != -1) {
#ifdef TREE_WITH_PIXELS
            std::array<int, 9> neighbors{-1,-1,-1,-1,255,-1,-1,-1,-1};
#else
            std::array<int, 9> neighbors{ 0, 0, 0, 0,255, 0, 0, 0, 0};
#endif
            assert(current_row->y != -1);
            assert(next_row->y != -1);
            
            int cy = current_row->y;
            
            const Vec2 center(offx + 0.5, current_row->y + 0.5);
            for(const auto& x : current_row->border) {
#ifdef TREE_WITH_PIXELS
                auto ptr = previous_row->pixels.data() + x - 1;
                auto ptr0 = current_row->pixels.data() + x - 1;
                auto ptr1 = next_row->pixels.data() + x - 1;
                
                if(previous_row->valid()) {
                    neighbors[indexes[TOPL]] = (x-1 >= 0 && (*previous_row)[x-1] == cy-1) ? *(ptr) : 0;
                    neighbors[indexes[TOP]] = (x >= 0 && (*previous_row)[x] == cy-1) ? *(ptr+1) : 0;
                    neighbors[indexes[TOPR]] = (x+1 >= 0 && (*previous_row)[x+1] == cy-1) ? *(ptr+2) : 0;
                } else
                    neighbors[indexes[TOP]] = neighbors[indexes[TOPL]] = neighbors[indexes[TOPR]] = 0;
                
                neighbors[indexes[LEFT]] = (x-1 >= 0 && (*current_row)[x-1] == cy) ? *(ptr0) : 0;
                neighbors[indexes[RIGHT]] = (x+1 >= 0 && (*current_row)[x+1] == cy) ? *(ptr0+2) : 0;
                
                neighbors[indexes[BOTTOML]] = (x-1 >= 0 && (*next_row)[x-1] == cy+1) ? *(ptr1) : 0;
                neighbors[indexes[BOTTOM]] = (x >= 0 && (*next_row)[x] == cy+1) ? *(ptr1+1) : 0;
                neighbors[indexes[BOTTOMR]] = (x+1 >= 0 && (*next_row)[x+1] == cy+1) ? *(ptr1+2) : 0;
#else
                
                // TOP, TOPR, RIGHT, BOTTOMR, BOTTOM, BOTTOML, LEFT, TOPL
                // 1,   2,    3+2,   6+2,     6+1,    6,       3,    0
                
                if(previous_row->valid()) {
                    neighbors[indexes[TOPL]] = (x-1 >= 0 && (*previous_row)[x-1] == cy-1) * 255;
                    neighbors[indexes[TOP]]  = (x >= 0 && (*previous_row)[x] == cy-1)     * 255;
                    neighbors[indexes[TOPR]] = (x+1 >= 0 && (*previous_row)[x+1] == cy-1) * 255;
                }
                
                neighbors[indexes[LEFT]]    = (x-1 >= 0 && (*current_row)[x-1] == cy) * 255;
                neighbors[indexes[RIGHT]]   = (x+1 >= 0 && (*current_row)[x+1] == cy) * 255;
                
                neighbors[indexes[BOTTOML]] = (x-1 >= 0 && (*next_row)[x-1] == cy+1) * 255;
                neighbors[indexes[BOTTOM]]  = (x   >= 0 && (*next_row)[x]   == cy+1) * 255;
                neighbors[indexes[BOTTOMR]] = (x+1 >= 0 && (*next_row)[x+1] == cy+1) * 255;
#endif
                
                tree.add(center.x + x, center.y, neighbors);
            }
        }
        
        //! pop previous_row, not needed anymore. switch current to previous row
        /// and next to current.
        std::swap(current_row, previous_row); // current = previous, previous = current
        std::swap(next_row, current_row);     // current = next, next = previous (empty)
        
        //! this is already the start of the next row, so initialize it
        next_row->range.start = x0;
        next_row->range.end = x1;
        next_row->y = y;
        next_row->border.clear();
    }
    
    std::vector<std::unique_ptr<std::vector<Vec2>>> find_outer_points(pv::BlobWeakPtr blob, int)
    {
        if(blob->hor_lines().empty())
            return {};
        
        int cols = blob->bounds().width + 2;
        int offx = blob->bounds().x;
        
#if !defined(NDEBUG) && (defined(DEBUG_TREE_WALK) || defined(DEBUG_EDGES))
        auto && [pos, image] = blob->binary_image();
        debug_greyscale = image;
#endif
        
        std::shared_ptr<Row>
            previous_row = std::make_shared<Row>(cols),
            current_row = std::make_shared<Row>(cols),
            next_row = std::make_shared<Row>(cols);
        
        pixel::Tree tree;
        
        int prev_x = -1, x0, x1;
        
#ifdef TREE_WITH_PIXELS
        const uchar* start;
        const uchar *px_ptr = blob->pixels()->data();
#endif
        
        //std::vector<std::shared_ptr<Row>> caches;
        
        //! We are iterating in the next line (i), but we keep three lines total
        /// in memory at all times (i-1 and i-2). In the beginning, those are obviously
        /// invalid. We only look at each horizontal line once.
        /// In order to determine border / non-border, we need the line above as well
        /// as the line below the focal line. We are only adding border pixels to the
        /// focal and the next line -- the previous line is really just readonly.
        
        for(auto &line : blob->hor_lines()) {
            x0 = int(line.x0) - offx;
            x1 = int(line.x1) - offx;
            
            if(next_row->y != line.y) {
                //! row ended, so we need to finalize it (e.g. pop queue)
                assert(next_row->y < line.y);
                //caches.push_back(std::make_shared<Row>(*next_row));
                finalize(offx, tree, previous_row, current_row, next_row, line.y, x0, x1);
                prev_x = -1;
                
            } else if(x0 - prev_x >= 1 && x0 >= current_row->range.start) {
                // write border pixels (if necessary) for previous row, if we skipped a few pixels
                // or if x0 is >= the start of the previous row
                for(int x=max(prev_x+1, current_row->range.start); x<x0 && x<=current_row->range.end; ++x) {
                    if((*current_row)[x] == next_row->y-1)
                        current_row->add_border(x); // next_row->y-1 ?
                }
            }
            
#ifdef TREE_WITH_PIXELS
            start = px_ptr;
            px_ptr += ptr_safe_t(x1) - ptr_safe_t(x0) + 1;
            std::copy(start, px_ptr, next_row->pixels.begin() + x0);
#endif
            
            if(x1 < current_row->range.start || !current_row->valid()) {
                // all pixels of the next line are border because the current ends
                // before the previous row even starts, or there is no current row
                for (int x=x0; x<=x1; ++x) {
                    next_row->add_border(x); // line.y ?
                    (*next_row)[x] = next_row->y;
                }
                
            } else {
                auto s = (int)x0;
                const auto& e = (int)x1; // just a rename
                
                if(s < current_row->range.start) {
                    // we have overlapping lines, fill up with until we either reach the current_row.start,
                    // or hit the end of the current line
                    // <-- next_row->range -[->    <-]- current_row->range -->
                    for (int x=s; x<current_row->range.start && x <= x1; ++x, ++s) {
                        next_row->add_border(x); // line.y
                        (*next_row)[x] = next_row->y;
                    }
                }
                else if(s == 0 || prev_x < s-1) {
                    // s is left-most, or we skipped a few frames, in which case this is the start border of a line
                    next_row->add_border(s); // line.y
                    (*next_row)[s] = next_row->y;
                    ++s;
                }
                
                // set border pixels from s(tart) to e(nd)
                for (; s<=e; ++s) {
                    // end of current line is def. a border
                    // 
                    if(s == e || s > current_row->range.end || (*current_row)[s] != current_row->y) {
                        // this pixel from next_row is not part of current_row, so its a border in next_row
                        next_row->add_border(s); // line.y
                    }
                    (*next_row)[s] = next_row->y;
                }
            }
            
            prev_x = x1;
            next_row->range.end = x1;
        }
        
        finalize(offx, tree, previous_row, current_row, next_row, next_row->y+1, 0, 0);
        finalize(offx, tree, previous_row, current_row, next_row, next_row->y+1, 0, 0);
        
/*#ifndef NDEBUG
        {
            int h = blob->bounds().height;
            Print("Height: ", h,", lines: ",caches.size());
            cv::Mat image = cv::Mat::zeros(h+2, blob->bounds().width+2, CV_8UC4);
            
            using namespace gui;
            for(auto &l : caches) {
                if(l->y == -1 || l->y >= image.rows)
                    continue;
                for(int x=0; x<l->cache.size(); ++x) {
                    assert(x < image.cols && l->y < image.rows && l->y >= 0);
                    image.at<cv::Vec4b>(l->y, x) = l->cache[x] == l->y ? Red : Gray;
                }
            }
            resize_image(image, 10);
            //if(blob->blob_id() == 5515881 || blob->parent_id() == 47907014)
            tf::imshow("array", image);
        }
#endif*/
        
        std::vector<std::unique_ptr<std::vector<Vec2>>> interp;
        try {
            interp = tree.generate_edges();
        } catch(const std::invalid_argument& e) {
            Print("Error");
            Print(blob->blob_id(),": ", e.what(), " ", blob->pixels() ? blob->pixels()->size() : 0);
            Print(blob->pixels() ? *blob->pixels() : PixelArray_t{}, "\n", blob->hor_lines(), "\n\n");
        }
        
        return interp;
    }
    
    bool Edge::operator<(const Edge& other) const {
        if(*A == *other.A) {
            if(*B == *other.B) {
                if (out_direction == other.out_direction) {
                    return in_direction < other.in_direction;
                } else
                    return out_direction < other.out_direction;
            } else
                return *B < *other.B;
        }
        
        return *A < *other.A;
    }
    bool Edge::operator>(const Edge& other) const {
        if(*A == *other.A) {
            if(*B == *other.B) {
                if (out_direction == other.out_direction) {
                    return in_direction > other.in_direction;
                } else
                    return out_direction > other.out_direction;
            } else
                return *B > *other.B;
        }
        
        return *A > *other.A;
    }
    bool Edge::operator==(const Edge& other) const {
        return out_direction == other.out_direction && in_direction == other.in_direction && *A == *other.A && *B == *other.B;
    }
    
    template<typename T>
    inline void add_edge(Tree::sides_t& _sides, T& non_full_nodes, const Edge& edge, const Tree::node_set_t& raw_nodes) {
        UNUSED(raw_nodes)
        /*auto it = edges.find(edge);
        if(it != edges.end()) {
            auto other = *it;
            throw U_EXCEPTION("Already contains edge with direction ",edge.out_direction,"->",edge.in_direction," from ",edge.A->position.x,",",edge.A->position.y," to ",edge.B->position.x,",",edge.B->position.y," (",it->B->position.x,",",it->B->position.y,")");
        }*/
        
        const auto& half_out = pixel::half_vectors[edge.out_direction];
        const auto& half_in = pixel::half_vectors[edge.in_direction];
        
        const Vec2 out(edge.A->x + half_out.x, edge.A->y + half_out.y);
        const Vec2 in(edge.B->x + half_in.x, edge.B->y + half_in.y);
        
#ifdef DEBUG_EDGES
        cv::Mat output;
        Vec2 mi(FLT_MAX), ma(0);

        for(auto && [idx, node] : non_full_nodes) {
            mi.x = min(node->position.x, mi.x);
            mi.y = min(node->position.y, mi.y);
            
            ma.x = max(node->position.x, ma.x);
            ma.y = max(node->position.y, ma.y);
        }
        
        for(auto && [idx, node] : _sides) {
            mi.x = min(node->position.x, mi.x);
            mi.y = min(node->position.y, mi.y);
            
            ma.x = max(node->position.x, ma.x);
            ma.y = max(node->position.y, ma.y);
        }
        
        if(mi.x == FLT_MAX)
            mi = Vec2(0);
        
        if(ma.x == ma.y && ma.x == 0)
            ma = Vec2(1);
        
        Bounds bounds(mi, Size2(ma - mi));
        Vec2 margin(1);
        float scale = 30;//1000.f / bounds.size().max();
        
        int img_height = bounds.height + margin.x*2, img_width = bounds.width + margin.y*2;
        output = cv::Mat::zeros(img_height * scale, img_width * scale, CV_8UC3);
        cv::Mat resized;
        //bounds.restrict_to(Bounds(Vec2(), Size2(debug_greyscale->get())));
        
        auto cropping = Bounds( mi-margin, Size2(img_width, img_height) + margin * 2);
        cropping.pos() = Vec2();
        //bounds.pos() = cropping.pos() + margin;
        //bounds.pos() += cropping.pos();
        cropping.restrict_to(Bounds(Vec2(), Size2(debug_greyscale->get())));
        //if(cropping.width > 0 && cropping.height > 0)
        //    debug_greyscale->get()(cropping).copyTo(output);
        //else
            debug_greyscale->get().copyTo(output);
        output *= 0.5;
        bounds.pos() = -mi + margin; //+ mi - margin;
        
        resize_image(output, output, scale);
        cv::cvtColor(output, output, cv::COLOR_GRAY2BGR);
        
#define OFFSET(X) (((X) + bounds.pos()) * scale)
        
        for (int x=0; x<debug_greyscale->cols; ++x) {
            cv::line(output, Vec2(x, 0) * scale, Vec2(x, debug_greyscale->rows) * scale, gui::White);
        }
        for (int y=0; y<debug_greyscale->rows; ++y) {
            cv::line(output, Vec2(0, y) * scale, Vec2(debug_greyscale->cols, y) * scale, gui::White);
        }
        
        using namespace gui;
        std::stringstream ss;
        for (auto && [idx, node] : non_full_nodes) {
            ss << "(" << node->position.x << "," << node->position.y << " l:";
            if(node->edges[0]) {
                ss << node->edges[0]->position.x << "," << node->edges[0]->position.y << ")";
            } else ss << "null";
            ss << " r:";
            if(node->edges[1]) {
                ss << node->edges[1]->position.x << "," << node->edges[1]->position.y << ")";
            } else ss << "null";
            ss << ") ";
            
            cv::circle(output, OFFSET(node->position), 0.2*scale, Cyan, -1);
        }
        
        for(auto node : raw_nodes) {
            for(auto x : node->border) {
                cv::circle(output, OFFSET(Vec2(node->x, node->y) + half_vectors[x]), 3, Cyan);
            }
        }
        
        for(auto && [idx, node] : _sides) {
            if(node->edges[0])
                cv::line(output, OFFSET(node->position), OFFSET(node->edges.at(0)->position), White);
            if(node->edges[1])
                cv::line(output, OFFSET(node->position), OFFSET(node->edges.at(1)->position), White);
            cv::circle(output, OFFSET(node->position), 0.2*scale, Red, -1);
        }
        
        cv::line(output, OFFSET(out), OFFSET(in), Red);
        
        auto str = ss.str();
        
        cv::cvtColor(output, output, cv::COLOR_BGR2RGB);
        cv::imshow("nodes", output);
        
        static int waitkey_method = 0;
        int key = cv::waitKey(waitkey_method);
        if(key == 13) {
            waitkey_method = 1;
        }
#endif
        
        decltype(non_full_nodes.begin()) it;
        uint64_t out_idx = Node::leaf_index(int64_t(out.x * 10), int32_t(out.y * 10));
        uint64_t in_idx = Node::leaf_index(int64_t(in.x * 10), int32_t(in.y * 10));
        
        Subnode *in_node  = nullptr,
                *out_node = nullptr;
        
        for(it = non_full_nodes.begin(); it != non_full_nodes.end();) {
            if((*it)->index == in_idx) {
                in_node = *it;
                _sides.push_back(*it);
                it = non_full_nodes.erase(it);
                
                if(in_node && out_node)
                    break;
                
            } else if((*it)->index == out_idx) {
                out_node = *it;
                _sides.push_back(*it);
                it = non_full_nodes.erase(it);
                
                if(in_node && out_node)
                    break;
                
            } else
                ++it;
        }
        
        auto found_out = out_node;
        if(!out_node) {
            out_node = new Subnode(out_idx, out, in_node);
            non_full_nodes.push_back(out_node);
        }
        
        if(!in_node) {
            in_node = new Subnode(in_idx, in, out_node);
            non_full_nodes.push_back(in_node);
            
            static_assert(int(bool(1234)) == 1, "Assume bool of arbitrary numbers != 0 is 1");
            out_node->edges[bool(found_out)] = in_node;
            
        } else if(found_out)
            out_node->edges[1] = in_node;
    }

Node::Node(float x, float y, const std::array<int, 9>& neighbors) : x(x), y(y), index(leaf_index(int64_t(x), int32_t(y))), neighbors(neighbors)
{ }

#define LEAF_INDEX(VEC) ( VEC )//Node::leaf_index( int64_t( VEC .x ), int32_t( VEC .y ) ) )

    void Tree::add(float x, float y, const std::array<int, 9> &neighborhood) {
#define IS_SET(NAME) (neighborhood[indexes[(size_t) NAME ]])
#define CHECK_BORDER(DIR) {if(!IS_SET(DIR)) node->border.insert(DIR); /* else node->neighbors.insert(DIR);*/ }
#define NAME(DIR) direction_names[ DIR ]
        
        auto node = std::make_unique<Node>(x, y, neighborhood);
        //node->position = offset;
        
#ifdef TREE_WITH_PIXELS
        Vec2 grad(0, 0);
        float norm = 0;
        float center = neighborhood[3+1];
        for (auto direction : directions) {
            float value = float(neighborhood[indexes[(size_t)direction]]) - center;
            grad += vectors[(size_t)direction] * 0.5 * value;
            norm += cmn::abs(value);
        }
        
        grad /= norm;
        node->gradient = grad;
#endif
        
        if (!neighborhood[1]) node->border[0] = true;
        else node->border[0] = false;
            //node->border.insert(TOP);
        if (!neighborhood[3]) node->border[1] = true;
        else node->border[1] = false;
            //node->border.insert(LEFT);
        if (!neighborhood[3+2]) node->border[2] = true;
        else node->border[2] = false;
            //node->border.insert(RIGHT);
        if (!neighborhood[6+1]) node->border[3] = true;
        else node->border[3] = false;
            //node->border.insert(BOTTOM);
        
        //_node_positions[node->index] = node;
        insert_sorted<std::unique_ptr<Node>>(_nodes, std::move(node),  Tree::Comparator{});
        //_nodes.emplace_back(std::move(node));
        //_nodes.emplace(std::move(node));
    }
    
    std::vector<std::unique_ptr<std::vector<Vec2>>> Tree::generate_edges() {
#undef IS_SET
#define IS_SET(NAME) (node->neighbors[ (size_t)indexes[NAME] ])
        
        //! we now have the pixel positions of all pixels with at least one border. now we have to find the half-pixel edges around these pixels and connect them.
        constexpr int max_val = (int)TOPL + 1;
        constexpr std::array<Direction, 4> direction_from_bool {
            TOP, LEFT, RIGHT, BOTTOM
        };
        
        static constexpr auto linear_search = [](const Vec2& v, auto &nodes) -> pixel::Node* {
            auto idx = Node::leaf_index(v.x, v.y);
            /*for(auto &node : nodes) {
                if(node->index == idx) {
                    return node.get();
                }
            }
            
            return nullptr;*/
            
            auto it = find_sorted<std::unique_ptr<Node>>(nodes, idx, Tree::Comparator{});
            //auto it = nodes.find(idx);
            /*auto it = std::find_if(nodes.begin(), nodes.end(), [idx](const auto& A) {
                return A->index == idx;
            });*/
            if(it == nodes.end())
                return nullptr;
            return it->get();
        };
        
        for(auto &node : _nodes) {
            const Vec2 offset(node->x, node->y);
            
            // node.positive_sides now contains all the sides with neighbors attached to them.
            // node.negative_sides contains all sides that dont have neighbors and will be interpolated.
            // these sides will need to get edges to other border pixels around the center.
            
            // check all borders (only main sides without diagonals)
            /*auto neighbor = node->neighbors; // copy neighbors array
            std::stringstream ss;
            ss << "[";
            for (auto border : node->border) {
                ss << direction_names[border] << ",";
            }
            ss << "]";
            auto str = ss.str();
            Print("Pixel at ", offset.x,",", offset.y," has borders: ",str);*/
            
            for (uchar i=0; i<node->border.size(); ++i) {
                if(!node->border[i])
                    continue;
                
                const auto &border = direction_from_bool[i];
                /**
                 * If we have a TOP border, for example, that means that there is no pixel to the left. However, on-pixels might still be located at the TOPLEFT or TOPRIGHT locations.
                 */
                int left = (int)border - 1;
                if(left < 0) left += max_val;
                
                // if border is TOP:
                //      left: border-1=TL, right: border+1=TR
                //      check: (left(TL, border-1), T(border) -> R(border+2)) : (left-1(L, border-2), T(border) -> T(border)),
                //             (right(TR, border+1), T(border) -> L(border-2) : (right+1(R, border+2), T(border) -> T(border))
                
                /** Test the left location for each border. If it is set, then the case is:
                 
                        TL pixel is set?
                          v
                        _ _ _ _ _
                       |  ??_    | < TOP is a border
                       |    o    | < center pixel
                       |_ _ _ _ _|
                 
                    The TL corner is checked. If true, then we go into this branch here:
                 */
                if(IS_SET(left)) {
                    /** It is true. So we definitely have a pixel at the TL. We construct an edge between the TLs pixels (TOP + 2) == RIGHT and our TOP.

                                _ _ _ _ _
                               |  o\_    |
                               |    o    |
                               |_ _ _ _ _|
                     
                     */
                    
                    int opposite = (int)border + 2;
                    if(opposite >= max_val) opposite -= max_val;
                    
                    auto v = offset + vectors[(size_t)left];
                    //Print("\t",NAME(left)," is set. add ",v.x,",",v.y," (",NAME(left)," with sides ",NAME(border)," -> ",NAME(opposite)," ",vectors[left].x,",",vectors[left].y,")");
                    auto ptr = linear_search(LEAF_INDEX(v), _nodes);
                    if(ptr)
                        add_edge(_sides, _non_full_nodes, Edge(border, (Direction)opposite, node.get(), ptr), _nodes);
                    
                } else {
                    /** It is false. So we dont have a pixel at TL. Maybe there is a pixel at (TOP-2) == LEFT?

                               _ _ _ _ _
                              |  ? _    |
                              |  ? o    |
                              |_ _ _ _ _|
                    
                    */
                    
                    int left_left = (int)border - 2;
                    if(left_left < 0) left_left += max_val;
                    
                    if(IS_SET(left_left)) {
                        /** So we do have a pixel at LEFT. Add an edge between TOP and TOP of the left pixel:

                                   _ _ _ _ _
                                  |  ___    |
                                  |  o o    |
                                  |_ _ _ _ _|
                        
                        */
                        
                        auto v = offset + vectors[left_left];
                        //Print("\t",NAME(left)," is not set, but ",NAME(left_left)," is. adding long route ",NAME(border)," -> (",v.x,",",v.y,") ",NAME(border));
                        
                        auto ptr = linear_search(LEAF_INDEX(v), _nodes);
                        if(ptr)
                            add_edge(_sides, _non_full_nodes, Edge(border, border, node.get(), ptr), _nodes);
                        
                    } else {
                        /** So we dont have a pixel at LEFT. This means we're on our own. Add a connection from our own TOP to LEFT.

                                   _ _ _ _ _
                                  |    _    |
                                  |   |o    |
                                  |_ _ _ _ _|
                        
                        */
                        
                        //Print("\t",NAME(left)," and ",NAME(left_left)," are not set. adding inner connection with ",NAME(border)," -> ",NAME(left_left));
                        add_edge(_sides, _non_full_nodes, Edge(border, (Direction)left_left, node.get(), node.get()), _nodes);
                    }
                }
            }
        }
        
        _sides.insert(_sides.end(), _non_full_nodes.begin(), _non_full_nodes.end());
        _non_full_nodes.clear();
        
        std::vector<std::unique_ptr<std::vector<Vec2>>> lines;
        for(auto& node : _sides) {
            if(!node->walked) {
                lines.emplace_back(walk(node));
            }
        }
        
        for(const auto& ptr : _sides)
            delete ptr;
        _sides.clear();
        
        return lines;
#undef IS_SET
    }
    
    std::unique_ptr<std::vector<Vec2>> Tree::walk(Subnode* node) {
        std::deque<pixel::Subnode*> q;
        auto line = std::make_unique<std::vector<Vec2>>();
        line->reserve(q.size() + 1);
        
        q.push_front(node);
        node->walked = true;
        
#ifdef DEBUG_TREE_WALK
        Vec2 mi(FLT_MAX, FLT_MAX);
        Vec2 ma(0, 0);
        
        for(auto &node : _nodes) {
            mi.x = min(mi.x, node->x);
            mi.y = min(mi.y, node->y);
            
            ma.x = max(ma.x, node->x);
            ma.y = max(ma.y, node->y);
        }
        
        Bounds bounds(mi, Size2(ma - mi));
        float scale = 2000 / bounds.size().max();
        Vec2 margin(1);
        
        cv::Mat output = cv::Mat::zeros((bounds.height+margin.y*2) * scale, (bounds.width+margin.x*2) * scale, CV_8UC3);
        
        Print("Dimensions: ",bounds.x,",",bounds.y," ",bounds.width,"x",bounds.height," (",output.cols,"x",output.rows,")");
        
        using namespace gui;
#endif
        
        while(!q.empty()) {
            node = q.front();
            q.pop_front();
            line->emplace_back(node->position);
            
#ifdef DEBUG_TREE_WALK
            output.setTo(0);
#define OFFSET(X) (((X) - bounds.pos() + margin) * scale)
            
            for(auto &node : _nodes) {
                cv::rectangle(output, OFFSET(Vec2(node->x,node->y) - Vec2(0.5)), OFFSET(Vec2(node->x,node->y) + Vec2(0.5)), Red, -1);
                
                for(auto side : node->border) {
                    auto center = pixel::half_vectors[side];
                    auto v0 = center;
                    auto d = Vec2(v0.y, -v0.x);
                    v0 = v0 - d; // transpose
                    auto v1 = v0 + d * 2;
                    cv::circle(output, OFFSET(Vec2(node->x,node->y) + center), 0.1 * scale, White, max(1, 0.025 * scale));
                    cv::rectangle(output, OFFSET(Vec2(node->x,node->y) + v0), OFFSET(Vec2(node->x,node->y) + v1), White, max(1, 0.025 * scale));
                }
                //cv::circle(output, OFFSET(node->position), 3, Cyan);
            }
            
            auto prev = line->back();
            for(auto pt : *line) {
                auto t = max(1, min(0.025 * scale, CV_MAX_THICKNESS));
                cv::line(output, OFFSET(prev), OFFSET(pt), DarkCyan, t);
                prev = pt;
            }
            cv::circle(output, OFFSET(node->position), 0.1 * scale, DarkCyan, max(1, 0.025 * scale));
            
            auto step_size = Size2(output) * 0.25;
            step_size.width = min(step_size.width, 250);
            step_size.height = min(step_size.height, 250);
            
            auto p0 = OFFSET(node->position) - step_size;
            auto p1 = OFFSET(node->position) + step_size;
            
            p0.x = max(p0.x, 0);
            p0.y = max(p0.y, 0);
            
            p1.x = min(output.cols, p1.x);
            p1.y = min(output.rows, p1.y);
            
            cv::cvtColor(output, output, cv::COLOR_BGR2RGB);
            tf::imshow("output", output);//(Bounds(p0, p1 - p0)));
            /*static int waitkey_method = 0;
            int key = cv::waitKey(waitkey_method);
            if(key == 13) {
                waitkey_method = 1;
            }*/
#endif
        
            for (auto& edge : node->edges) {
                //auto idx = std::tuple<int, int>{edge->position.x*10, edge->position.y*10};
                if(!edge ||edge->walked)
                    continue;
                
                edge->walked = true;
                q.push_front(edge);
            }
        }
        return line;
    }
}
