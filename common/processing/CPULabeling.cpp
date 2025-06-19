#include "CPULabeling.h"
#include <misc/GlobalSettings.h>
#include <misc/Timer.h>
#include <misc/pretty.h>
#include <misc/ranges.h>
#include <misc/ThreadPool.h>
#include <misc/PVBlob.h>
#include <processing/Source.h>
#include <processing/DLList.h>
#include <processing/Brototype.h>
#include <processing/ListCache.h>

namespace cmn {
namespace CPULabeling {

//#define DEBUG_MEM

/*void Node::Ref::release_check() {
    if(!obj)
        return;
    
    obj->release();
    if(obj->unique()) {
        if(obj->parent) {
            obj->parent->cache().receive(Ref(obj));
        } else
            delete obj;
    }
    obj = nullptr;
}*/

/**
 * Merges arrays of HorizontalLines into Blobs.
 * This function expects to be given a previous line and the focal line in
 * the source image (processed, so that the input are actually arrays of HorizontalLines).
 * It then tries to merge the HorizontalLines within the current line with the HL in the
 * previous line, if they are connected, and inserts them into Blobs.
 * It also merges Blobs if necessary.
 *
 * @param previous_vector contains all HorizontalLines from y-1
 * @param current_vector contains all HorizontalLines from y
 * @param blobs output / input vector for the currently existing Blobs
 */
void merge_lines(Source::RowRef &previous_vector,
                 Source::RowRef &current_vector,
                 List_t &blobs)
{
    // walk along both lines, trying to match pairs
    auto current = current_vector.begin();
    auto previous = previous_vector.begin();
    auto current_end = current_vector.end();
    auto previous_end = previous_vector.end();
    size_t hint = std::distance(current_vector.begin(), current_vector.end());
    // Use the current_vector size as a hint; could be smarter if you have better info
    size_t reserve_hint = std::max<size_t>(64, hint);
    
    while (current != current_end) {
        auto curr_lit = current->Lit;
        auto prev_lit = (previous != previous_end) ? previous->Lit : nullptr;
        if(prev_lit == nullptr
           || curr_lit->line.y() > (prev_lit->line).y() + 1
           || curr_lit->line.x1()+1 < (prev_lit->line).x0())
        {
            // case 0: current line ends before previous line starts
            // case 1: lines are more than 1 apart in y direction,
            // case 2: the previous line has ended before the current line started-1
            //
            // -> create new blobs for all elements
            // add new blob, next element in current line
            assert(prev_lit == nullptr
                   || (curr_lit)->line.y() > (prev_lit)->line.y() + 1
                   || !(curr_lit->line).overlap_x(prev_lit->line));
            
            if(!(curr_lit)->node
               || !(curr_lit)->node->parent)
            {
                auto p = blobs.cache().broto();
                if(p)
                    p->push_back(curr_lit->line, *current->Pit);
                else {
                    p = std::make_unique<Brototype>(reserve_hint);
                    p->push_back(curr_lit->line, *current->Pit);
                }
                
                blobs.insert((curr_lit->node), std::move(p));
                
            }
            
            ++current;
            
        } else if((curr_lit->line).x0() > (prev_lit->line).x1()+1) {
            // case 3: previous line ends before current
            // next element in previous line
            assert(!(curr_lit->line).overlap_x(prev_lit->line));
            ++previous;
            
        } else {
            // case 4: lines intersect
            // merge elements, next in line that ends first
            assert((curr_lit->line).overlap_x(prev_lit->line));
            assert(prev_lit->node);
            
            auto pblob = (prev_lit->node);
            
            if(!curr_lit->node) {
                // current line isnt part of a blob yet
                // nit is null!
                pblob->obj->push_back((curr_lit->line), *current->Pit);
                curr_lit->node = (prev_lit->node);
                
            } else if(curr_lit->node != prev_lit->node) {
                // current line is part of a blob
                // (merge blobs)
                assert(curr_lit->node != prev_lit->node);
                
                auto p = previous;
                auto c = current;
                
                // copy all lines from the blob in "current"
                // into the blob in "previous"
                if((p->Lit->node)->obj->size() <= (c->Lit->node)->obj->size()) {
                    std::swap(p, c);
                }
                
                auto cblob = (c->Lit->node);
                auto pblob = (p->Lit->node);
                
                assert(cblob != pblob);
                
                if(!cblob->obj->empty())
                    pblob->obj->merge_with(cblob->obj);
                
                // replace blob pointers in current_ and previous_vector
                for(auto cit = current_vector.begin(); cit != current_end; ++cit) {
                    auto cit_lit = cit->Lit;
                    auto p_lit = p->Lit;
                    if(cit_lit->node == cblob) {
                        cit_lit->node = p_lit->node;
                    }
                }
                
                for(auto cit = previous; cit != previous_end; ++cit) {
                    auto cit_lit = cit->Lit;
                    auto p_lit = p->Lit;
                    if(cit_lit->node == cblob) {
                        cit_lit->node = p_lit->node;
                    }
                }
                
                if(cblob->obj)
                    Brototype::move_to_cache(&blobs, cblob->obj);
                Node_t::move_to_cache(cblob);
            }
            
            /*
             * increase the line pointer of the line that
             * ends first. given the following situation
             *
             * previous  >|---------|  |------|      |----|
             * current         >|-------|   |-----|
             *
             * previous would be increased and the line
             * following it can be merged with current->first
             * as well. (current line connects n previous lines)
             *
             * in the following steps it would increase current
             * and then terminate.
             */
            if((curr_lit->line).x1() <= (prev_lit->line).x1())
                ++current;
            else
                ++previous;
        }
    }
}

blobs_t run_fast(List_t* blobs, ptr_safe_t channels)
{
    blobs_t result;
    auto& source = blobs->source();
    const uint8_t initial_flags = pv::Blob::get_only_flag(pv::Blob::Flags::is_rgb, channels == 3);
    
    if(source.empty())
        return {};
    
    /**
     * SORT HORIZONTAL LINES INTO BLOBS
     * tested cases:
     *      - empty image
     *      - only one line
     *      - only two lines (one HorizontalLine in each of the two y-arrays)
     */
        
    // iterators for current row (y-coordinate in the original image)
    // and for the previous row (the last one above current that contains objects)
    auto current_row = source.row(0);
    auto previous_row = source.row(0);
    
    // create blobs for the current line if its not empty
    if(current_row.valid()) {
        auto start = current_row.begin();
        auto end = current_row.end();
        for(auto it = start; it != end; ++it) {
            auto &[o,l,p] = *it;
            auto bob = blobs->cache().broto();
            if(!bob)
                bob = std::make_unique<Brototype>(l->line, *p);
            else
                bob->push_back(l->line, *p);
            
            blobs->insert(l->node, std::move(bob));
        }
    }
    
    // previous_row remains the same, but current_row has to go to the next one (all blobs for the first row have already been created):
    current_row.inc_row();
    
    // loop until the current_row iterator reaches the end of all arrays
    while (previous_row.valid()) {
        merge_lines(previous_row, current_row, *blobs);
        
        previous_row = current_row;
        current_row.inc_row();
    }
    
    /**
     * FILTER BLOBS FOR SIZE, transform them into proper format
     */
    result.reserve(std::distance(blobs->begin(), blobs->end()));

    //! TODO: Do not store actual pixels in an object. Only store start pointers per Horizontal Line and delete image information when done with it [OPT]
    for(auto it=blobs->begin(); it != blobs->end(); ++it) {
        //! skip empty objects / merged objects
        if(!it->obj || it->obj->empty())
            continue;
        
        result.emplace_back(std::make_unique<std::vector<HorizontalLine>>(),
                            std::make_unique<std::vector<uchar>>(),
                            initial_flags);
        
        auto &lines = result.back().lines;
        auto &pixels = result.back().pixels;
        
        ptr_safe_t L = 0;
        for(auto & [l, px] : *it->obj) {
            assert(l->x1() >= l->x0());
            L += ptr_safe_t((l)->x1()) - ptr_safe_t((l)->x0()) + ptr_safe_t(1);
        }
        
        pixels->resize(L * channels);
        auto LLines = it->obj->lines().size();
        lines->resize(LLines);
        
#ifndef NDEBUG
        coord_t y = 0;
#endif
        auto current = lines->data();
        auto pixel = pixels->data();
        for(auto & [l, px] : *it->obj) {
            auto lx0 = l->x0();
            auto lx1 = l->x1();
            auto ly = l->y();
            assert(ly >= y);
            //if(ptr_safe_t(l->x1()) - ptr_safe_t(l->x0()) >= 63)
                //Print("Line is of suspicious length: ", *l);
                
            if(current > lines->data()
               && (current - 1)->x1 + 1 == lx0
               && (current - 1)->y == ly)
            {
                //Print("Could merge lines ", *l, " and ", *(current-1));
                (current-1)->x1 = lx1;
                --LLines;
            } else {
                *current++ = *l; // assign **l to *current; inc current
            }
            
            if(pixels) {
                assert(*px);
                auto start = *px;
                auto end = start + ((ptr_safe_t(lx1) - ptr_safe_t(lx0) + ptr_safe_t(1))) * channels;
                
                //! HOTSPOT
                pixel = std::copy(start, end, pixel);
            }
            
#ifndef NDEBUG
            y = ly;
#endif
        }
        
        if(LLines != lines->size())
            lines->resize(LLines);
        
        Node_t::move_to_cache(*it);
    }
    
#if defined(DEBUG_MEM)
    if(!Brototype::brototypes().empty())
        Print("Still have ",Brototype::brototypes().size()," brototypes in memory");
    
    source._nodes.clear();
    
    {
        Node_t::pool().clear();
        
        std::lock_guard guard(Node_t::mutex());
        if(!Node_t::created_nodes().empty())
            Print("Still have ",Node_t::created_nodes().size()," nodes in memory");
    }
    
    Brototype::brototypes().clear();
#endif
    
    return result;
}

// called by user
blobs_t run(DLList& list, const cv::Mat &image, bool enable_threads) {
    //auto list = List_t::from_cache();
    //DLList list;
    list.clear();
    list.source().init(image, enable_threads);
    
    blobs_t results = run_fast(&list, image.channels());
    //List_t::to_cache(std::move(list));
    list.clear();
    return results;
}

blobs_t run(const cv::Mat &image, ListCache_t& cache, bool enable_threads) {
    //auto list = List_t::from_cache();
    //DLList list;
    cache.obj->source().init(image, enable_threads);
    return run_fast(cache.obj, image.channels());
    //List_t::to_cache(std::move(list));
    //return results;
}

// called by user
blobs_t run(const std::vector<HorizontalLine>& lines, const std::vector<uchar>& pixels, ListCache_t& cache, uint8_t channels)
{
    auto px = pixels.data();
    //list.clear();
    //List_t list;
    auto& list = *cache.obj;
    list.clear();
    list.source().reserve(lines.size());

    auto start = lines.begin();
    auto end = lines.end();

    for(auto it = start; it != end; ++it) {
        if(ptr_safe_t(it->x1) - ptr_safe_t(it->x0) > Line_t::bit_size_x1) {
            auto pxcopy = px;
            auto copy = *it;
            while(ptr_safe_t(copy.x1) - ptr_safe_t(copy.x0) > Line_t::bit_size_x1) {
                Line_t line(copy.x0, copy.x0 + Line_t::bit_size_x1, copy.y);
                //Print("converting ", *it, " to shorter lines: ", line.x0(), ",", line.x1(), ";", line.y());
                list.source().push_back(line, pxcopy);

                copy.x0 += Line_t::bit_size_x1 + 1;
                pxcopy += (Line_t::bit_size_x1 + 1) * ptr_safe_t(channels);
            }

            //Print("converting ", *it, " to shorter lines: ", copy);
            list.source().push_back(copy, pxcopy);

        } else
            list.source().push_back((*it), px);

        if(px)
            px += (ptr_safe_t(it->x1) - ptr_safe_t(it->x0) + ptr_safe_t(1)) * ptr_safe_t(channels);
    }

    return run_fast(&list, channels);
}

blobs_t run(const std::vector<HorizontalLine>& lines, const std::vector<uchar>& pixels, uint8_t channels)
{
    if(lines.empty())
        return {};
    
    ListCache_t cache;
    //auto list = List_t::from_cache();
    //List_t::to_cache(std::move(list));
    return run(lines, pixels, cache, channels);
}

}
}
