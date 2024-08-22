#pragma once
#include <commons.pc.h>

namespace cmn {
    namespace curves {
        struct Extrema {
            std::vector<Float2_t> minima;
            std::vector<Float2_t> maxima;
            
            Float2_t min, max, mean;
        };

        std::vector<Float2_t> derive(const std::vector<Float2_t>& values);
        Extrema find_extreme_points(const std::vector<Float2_t>& values, std::vector<Float2_t>& derivative);

        std::map<Float2_t, Float2_t> area_under_minima(const std::vector<Float2_t>& values);
        std::map<Float2_t, Float2_t> area_under_maxima(const std::vector<Float2_t>& values);
        
        std::map<Float2_t, Float2_t> area_under_minima(const std::vector<Float2_t>& values, std::vector<Float2_t>& derivative);
        std::map<Float2_t, Float2_t> area_under_maxima(const std::vector<Float2_t>& values, std::vector<Float2_t>& derivative);
        
        Float2_t interpolate(const std::vector<Float2_t>& values, Float2_t index);
    }
}
