#pragma once

#include <misc/Grid.h>
#include <misc/bid.h>

namespace std
{
    template <typename A, typename B>
    struct hash<std::tuple<A, B>>
    {
        size_t operator()(const std::tuple<A, B>& k) const
        {
            return ((hash<A>()(std::get<0>(k)) ^ (hash<B>()(std::get<1>(k)) << 1)) >> 1);
        }
    };

    template <typename A, typename B, typename C>
    struct hash<std::tuple<A, B, C>>
    {
        size_t operator()(const std::tuple<A, B, C>& k) const
        {
            return ((hash<A>()(std::get<0>(k)) ^ (hash<B>()(std::get<1>(k)) << 1) ^ (hash<C>()(std::get<2>(k)) << 2)) >> 1);
        }
    };
}

namespace cmn {
namespace grid {

using fdx_pos = pv::bid;
static constexpr int proximity_res = 100;

class ProximityGrid : public Grid2D<fdx_pos, std::vector<pixel<fdx_pos>>> {
public:
    using result_t = std::tuple<float, fdx_pos>;
    
    ProximityGrid() noexcept = default;
    ProximityGrid(const Size2& resolution, int r = -1);
    
    UnorderedVectorSet<result_t> query(const Vec2& pos, float max_d) const;
    std::string str(fdx_pos dx, Vec2 point, float max_d) const;
    
private:
    virtual fdx_pos query(float, float) const override;
};

}
}
