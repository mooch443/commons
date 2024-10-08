#pragma once

#include <commons.pc.h>
#include <misc/Median.h>
#include <misc/ranges.h>

namespace cmn {
    namespace periodic {
        typedef Float2_t scalar_t;
        typedef Vec2 point_t;
        
        struct Vec4 {
            scalar_t x, y, z, w;
        };
        
        typedef std::unique_ptr<std::vector<point_t>> points_t;
        typedef std::unique_ptr<std::vector<scalar_t>> scalars_t;
        typedef std::unique_ptr<std::vector<Vec4>> coeff_t;
        typedef Range<scalar_t> range_t;
        
        struct Peak {
            Vec2 position;
            scalar_t width, integral;
            range_t range;
            bool maximum;
            scalar_t max_y_extrema;
            
            //std::vector<range_t> split_ranges;
            std::vector<Vec2> points;
            //Median<scalar_t> median_y;
            scalar_t max_y;
            
            Peak(Vec2 pos, scalar_t width = 0, range_t range = range_t(-1, -1))
                : position(pos), width(width), integral(0), range(range), max_y_extrema(0), max_y(0)
            {
                
            }
            
            bool operator>(const Peak& other) const {
                return position.y > other.position.y;
            }
            
            bool operator<(const Peak& other) const {
                return position < other.position;
            }
            
            std::string toStr() const;
            static std::string class_name() {
                return "Peak";
            }
        };
        
        typedef std::unique_ptr<std::vector<Peak>> peaks_t;
        
        scalars_t curvature(const points_t::element_type&, int, bool = false);
        
        //! calculates out[n] = a[n+1] - a[n] for every point
    std::vector<points_t> differentiate(const points_t::element_type&, size_t times = 1);
    std::vector<scalars_t> differentiate(const scalars_t::element_type&, size_t times = 1);
    std::tuple<scalar_t, std::vector<points_t>> differentiate_and_test_clockwise(const points_t::element_type&, size_t times = 1);
        
        enum PeakMode {
            FIND_BROAD,
            FIND_POINTY
        };
        std::tuple<peaks_t, peaks_t> find_peaks(const scalars_t&, float min_width = 0, const std::vector<scalars_t>& = {}, PeakMode = FIND_BROAD);
        
        coeff_t eft(const points_t::element_type&, size_t order = 10, const points_t& derivative = nullptr);
        std::vector<points_t> ieft(const coeff_t::element_type&, size_t order, size_t n_points, Vec2 offset = Vec2(0), bool save_steps = true, Float2_t scale = 1_F);
        
        namespace EFT {
            std::tuple<points_t, scalars_t, scalars_t, scalars_t> dt(const points_t& derivative);
        }

        class Curve {
        public:
            typedef std::unique_ptr<Curve> Ptr;
            
        protected:
            GETTER(bool, is_clockwise);
            GETTER(points_t, points);
            
            GETTER(std::vector<points_t>, derivatives);
            scalars_t _curvature;
            
        public:
            Curve();
            
            // takes ownership of points and sets them
            void set_points(points_t&& points, bool copy = true);
            void make_clockwise();
            
            // Curve::Ptr approximate(uint32_t order);
        };
    }
}
