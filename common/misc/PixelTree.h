#pragma once

#include <commons.pc.h>
#include <misc/bid.h>
#include <misc/ranges.h>
#include <processing/Background.h>
#include <processing/CPULabeling.h>

//#define TREE_WITH_PIXELS

namespace cmn::pixel {
    // 8-neighborhood
    enum Direction {
        TOP = 0, TOPR,
        RIGHT, BOTTOMR,
        BOTTOM, BOTTOML,
        LEFT, TOPL
    };
    
    constexpr std::array<Direction, TOPL+1> directions {
        TOP, TOPR, RIGHT, BOTTOMR, BOTTOM, BOTTOML, LEFT, TOPL
    };
    
    // maps Direction(s) to index in the 3x3 neighborhood array
    constexpr std::array<size_t, 8> indexes {
        1, 2, 3+2, 6+2, 6+1, 6, 3, 0
    };
    
    // maps Direction(s) to offset-vectors
    constexpr std::array<Vec2, 8> vectors {
        Vec2(0,-1), Vec2(1,-1), Vec2(1,0), Vec2(1,1), Vec2(0,1), Vec2(-1,1), Vec2(-1,0), Vec2(-1,-1)
    };
    constexpr std::array<Vec2, 8> half_vectors {
        Vec2(0,-1) * 0.5, Vec2(1,-1) * 0.5, Vec2(1,0) * 0.5, Vec2(1,1) * 0.5, Vec2(0,1) * 0.5, Vec2(-1,1) * 0.5, Vec2(-1,0) * 0.5, Vec2(-1,-1) * 0.5
    };
    
    constexpr std::array<const char*, 8> direction_names {
        "T", "TR", "R", "BR", "B", "BL", "L", "TL"
    };
    
    class Node;
    
    struct Edge {
        Direction out_direction, in_direction;
        Node *A, *B;
        
        Edge(Direction dout = TOP, Direction din = TOP, Node* A = nullptr, Node* B = nullptr)
            : out_direction(dout), in_direction(din), A(A), B(B)
        {}
        
        bool operator<(const Edge& other) const;
        bool operator>(const Edge& other) const;
        bool operator==(const Edge& other) const;
    };
    
    class Subnode {
    public:
        Vec2 position;
        std::array<Subnode*, 2> edges;
        bool walked;
        uint64_t index;
        
        Subnode() : walked(false), index(0) {
            edges[0] = edges[1] = nullptr;
        }
        
        Subnode(uint64_t index, const Vec2& position, Subnode* first) : position(position), walked(false), index(index) {
            edges[0] = first;
            edges[1] = nullptr;
        }
    };
    
    class Subtree {
    public:
        
    };
    
    class Node {
    public:
        float x,y;
        uint64_t index;
        
#ifdef TREE_WITH_PIXELS
        Vec2 gradient; // pixel value gradient
#endif
        std::array<bool, 4> border; // all main-sides that have neighbors
        std::array<int, 9> neighbors; // all main-sides that dont have neighbors
        
        Node(float x, float y, const std::array<int, 9>& neighbors = {0});
        
        bool operator<(const Node& other) const {
            return y < other.y || (y == other.y && x < other.x);
        }
        bool operator>(const Node& other) const {
            return !(operator<(other));
        }
        bool operator==(const Node& other) const {
            return x == other.x && y == other.y;
        }
        
        constexpr static uint64_t leaf_index(int64_t x, int32_t y) {
            return uint64_t( ( (uint64_t(x) << 32) & 0xFFFFFFFF00000000 ) | (uint64_t(y) & 0x00000000FFFFFFFF) );
        }
    };
    
    class Tree {
    protected:
        GETTER(std::vector<std::unique_ptr<Node>>, nodes);
        
    public:
        using sides_t = std::vector<Subnode*>;
    protected:
        GETTER(sides_t, sides);
        std::vector<Subnode*> _non_full_nodes;
        
    public:
        void add(float x, float y, const std::array<int, 9>& neighborhood);
        std::vector<std::shared_ptr<std::vector<Vec2>>> generate_edges();
        
    private:
        std::shared_ptr<std::vector<Vec2>> walk(Subnode* node);
    };
    
    std::vector<std::shared_ptr<std::vector<Vec2>>> find_outer_points(pv::BlobWeakPtr blob, int threshold);

    pv::BlobPtr threshold_get_biggest_blob(pv::BlobWeakPtr blob, int threshold, const cmn::Background* bg, uint8_t use_closing = 0, uint8_t closing_size = 2);
    //std::vector<pv::BlobPtr> threshold_blob(pv::BlobWeakPtr blob, int threshold, const cmn::Background* bg, const Rangel& size_range = Rangel(-1, -1));
    std::vector<pv::BlobPtr> threshold_blob(CPULabeling::ListCache_t&, pv::BlobWeakPtr blob, int threshold, const cmn::Background* bg, const Rangel& size_range = Rangel(-1,-1));

    std::vector<pv::BlobPtr> threshold_blob(CPULabeling::ListCache_t&, pv::BlobWeakPtr blob, const std::vector<uchar>& difference_cache, int threshold, const Background& background, const Rangel& size_range = Rangel(-1,-1));
    //std::vector<pv::BlobPtr> threshold_blob(pv::BlobWeakPtr blob, const std::vector<uchar>& difference_cache, int threshold, const Rangel& size_range = Rangel(-1,-1));

#define _____FN_TYPE (const Background* bg, const std::vector<HorizontalLine>& lines, uchar*& px, int threshold, std::vector<HorizontalLine> &result, std::vector<uchar> &pixels)

    template<InputInfo input, OutputInfo output, DifferenceMethod method>
    inline void line_with_grid _____FN_TYPE {
        for(const auto &line : lines) {
            coord_t x0;
            uchar* start{nullptr};
            auto threshold_ptr = bg->grid()->thresholds().data() + ptr_safe_t(line.x0) + ptr_safe_t(line.y) * ptr_safe_t(bg->grid()->bounds().width);
            
            for (auto x=line.x0; x<=line.x1; ++x, px += input.channels) {
                //assert(px < px_end);
                if(bg->diff<output, method>(x, line.y, *px) < (*threshold_ptr++) * threshold) {
                    if(start) {
                        pixels.insert(pixels.end(), start, px);
                        result.emplace_back(line.y, x0, x - 1);
                        start = nullptr;
                    }
                    
                } else if(!start) {
                    start = px;
                    x0 = x;
                }
            }
        
            if(start) {
                pixels.insert(pixels.end(), start, px);
                result.emplace_back(line.y, x0, line.x1);
            }
        }
    }

    template<InputInfo input, OutputInfo output, DifferenceMethod method>
    inline void line_without_grid _____FN_TYPE {
        for(const auto &line : lines) {
            coord_t x0;
            uchar* start{nullptr};
            
            for (auto x=line.x0; x<=line.x1; ++x, px += input.channels) {
                //assert(px < px_end);
                
                auto [pixel_value, grey_value] = dual_diffable_pixel_value<input, output>(px);
                if(not bg->is_different<DIFFERENCE_OUTPUT_FORMAT, method>(x, line.y, grey_value, threshold)) {
                    if(start) {
                        pixels.insert(pixels.end(), start, px);
                        result.emplace_back(line.y, x0, x - 1);
                        start = nullptr;
                    }
                    
                } else if(!start) {
                    start = px;
                    x0 = x;
                }
            }
        
            if(start) {
                pixels.insert(pixels.end(), start, px);
                result.emplace_back(line.y, x0, line.x1);
            }
        }
    }

    template<InputInfo input, OutputInfo output, DifferenceMethod method>
    inline void line_without_bg _____FN_TYPE {
        UNUSED(bg);
        
        for(const auto &line : lines) {
            coord_t x0;
            uchar* start{nullptr};
            
            for (auto x=line.x0; x<=line.x1; ++x, px += input.channels) {
                //assert(px < px_end);
                
                if(*px < threshold) {
                    if(start) {
                        pixels.insert(pixels.end(), start, px);
                        result.emplace_back(line.y, x0, x - 1);
                        start = nullptr;
                    }
                    
                } else if(!start) {
                    start = px;
                    x0 = x;
                }
            }
        
            if(start) {
                pixels.insert(pixels.end(), start, px);
                result.emplace_back(line.y, x0, line.x1);
            }
        }
    }
#undef _____FN_TYPE
}
