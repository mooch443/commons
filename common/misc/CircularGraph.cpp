#include "CircularGraph.h"
#include <misc/Timer.h>

#define EXPERIMENTAL_FAST_COSINE
#ifdef EXPERIMENTAL_FAST_COSINE

namespace fast {

constexpr const double F_PI       = 3.14159265358979323846264338327950288;
constexpr const double F_PI_2     = 1.57079632679489661923132169163975144;

template<typename T>
    requires std::floating_point<T>
inline T cos(T x) noexcept
{
    constexpr T tp = 1./(2. * F_PI);
    x *= tp;
    x -= T(.25) + std::floor(x + T(.25));
    x *= T(16.) * (std::abs(x) - T(.5));
//#if EXTRA_PRECISION
    x += T(.225) * x * (std::abs(x) - T(1.));
//#endif
    return x;
}

template<typename T>
    requires std::floating_point<T>
constexpr T sin(T x) noexcept {
    return fast::cos<T>(x - T(F_PI_2));
}

}
#else
namespace fast = std;
#endif

namespace cmn {
    namespace periodic {
        std::string Peak::toStr() const {
            return "Peak<"+Meta::toStr(position)+" w:"+Meta::toStr(width)+" i:"+Meta::toStr(integral)+" r:"+Meta::toStr(range)+">";
        }
        
        /**
         * This calculates the curvature of an array of outline points.
         * This is based on the fact that the curvature of a circle through
         * three consecutive points is simply the area of the triangle formed
         * by these points, divided by the product of its sides.
         *
         * The curvature returned is missing a sqrt() in the normalization
         * for performance reasons.
         */
        scalars_t curvature(const points_t::element_type& points, int r, bool absolute) {
            if (r < 1)
                throw std::invalid_argument("r must be >= 1");
            
            if(points.empty()) {
                FormatWarning("[curvature] points cannot be empty or nullptr.");
                return nullptr;
            }
            
            static Timing timing("curvature", 10);
            TakeTiming take(timing);
            
            scalars_t result = std::make_unique<scalars_t::element_type>();
            result->resize(points.size());
            
            auto start = points.data();
            auto end = start + points.size();
            
            auto ptr1 = end - r;
            auto ptr2 = start;
            auto ptr3 = start + r;
            
            auto out = result->data();
            
            if(absolute) {
                for (; ptr2 != end; ++ptr1, ++ptr2, ++ptr3, ++out) {
                    if(ptr1 == end)
                        ptr1 = start;
                    if(ptr3 == end)
                        ptr3 = start;
                    
                    const auto& p1 = *ptr1;
                    const auto& p2 = *ptr2;
                    const auto& p3 = *ptr3;
                    
                    if(p1 != p2 && p1 != p3 && p2 != p3)
                        *out = 2.f * std::abs((p2.x-p1.x)*(p3.y-p2.y) - (p2.y-p1.y)*(p3.x-p2.x))
                        / std::sqrt(sqdistance(p1, p2) * sqdistance(p2, p3) * sqdistance(p1, p3));
                }
            } else {
                for (; ptr2 != end; ++ptr1, ++ptr2, ++ptr3, ++out) {
                    if(ptr1 == end)
                        ptr1 = start;
                    if(ptr3 == end)
                        ptr3 = start;
                    
                    const auto& p1 = *ptr1;
                    const auto& p2 = *ptr2;
                    const auto& p3 = *ptr3;
                    
                    if(p1 != p2 && p1 != p3 && p2 != p3)
                        *out = 2.f * ((p2.x-p1.x)*(p3.y-p2.y) - (p2.y-p1.y)*(p3.x-p2.x))
                        / std::sqrt(sqdistance(p1, p2) * sqdistance(p2, p3) * sqdistance(p1, p3));
                }
            }
            
            return result;
        }
        
        std::tuple<peaks_t, peaks_t> find_peaks(const scalars_t& points, float, const std::vector<scalars_t>& diffs, PeakMode mode) {
            //static Timing timing("find_peaks", 0.01);
            //TakeTiming take(timing);
            
            if(!points || points->empty())
                return {nullptr, nullptr};
            
            const scalars_t::element_type *diff, *second_diff;
            
            std::vector<scalars_t> tmp;
            if(diffs.empty()) {
                auto diffs = differentiate(*points, 2);
                tmp.emplace_back(std::move(diffs.at(0)));
                tmp.emplace_back(std::move(diffs.at(1)));
                diff = tmp.front().get();
                second_diff = tmp.back().get();
                
            } else {
                auto L = diffs.size();
                if(L > 0)
                    diff = diffs.at(0).get();
                else {
                    tmp.emplace_back(std::move(differentiate(*points, 1).front()));
                    diff = tmp.back().get();
                }
                
                if(L > 1)
                    second_diff = diffs.at(1).get();
                else {
                    tmp.emplace_back(std::move(differentiate(*diff, 1).front()));
                    second_diff = tmp.back().get();
                }
            }
            
            bool sign = diff->back() < 0;
            auto ptr = diff->data();
            auto end = diff->data() + diff->size();
            
            auto ptr_second = second_diff->data();
            
            //auto prev = ptr;
            
            auto point = points->data();
            //auto prev_point = points->back();
            
            auto maxima = std::make_unique<peaks_t::element_type>();
            auto minima = std::make_unique<peaks_t::element_type>();
            
            std::set<std::tuple<scalar_t, size_t>, std::greater<>> sorted;
            std::set<std::tuple<scalar_t, bool>> extrema;
            
            size_t i=0;
            constexpr scalar_t n_min = -std::numeric_limits<scalar_t>::max(),
                               n_max = std::numeric_limits<scalar_t>::max();
            static_assert(n_min < 0, "assuming scalar_t to not be unsigned.");
            
            scalar_t minimum = n_max, maximum = n_min;
            scalar_t min_y = n_max;
            
            for (; ptr != end; ++ptr, ++ptr_second, ++point, ++i) {
                bool c = *ptr < 0;
                if(*point < minimum) {
                    minimum = *point;
                }
                if(*point > maximum) {
                    maximum = *point;
                }
                
                if (c != sign) {
                    // dont use the wendepunkt!
                    if(*ptr_second != 0) {
                        Peak peak(Vec2(i, *point));
                        if(!sign) {
                            peak.maximum = true;
                            maxima->push_back(peak);
                            sorted.insert({peak.position.y, maxima->size()-1});
                            
                        } else {
                            peak.maximum = false;
                            minima->push_back(peak);
                            
                        }
                        extrema.insert({i, peak.maximum});
                    }
                    
                    sign = c;
                }
            }
            
            min_y = minimum;
            std::vector<range_t> ranges;
            
            for(auto it=sorted.begin(); it != sorted.end(); ++it) {
                auto &peak = maxima->at(std::get<1>(*it));
                
                // find right neighbor
                auto after = extrema.begin();
                auto prev = --extrema.end();
                for (; after != extrema.end(); ++after) {
                    if(std::get<0>(*after) == peak.position.x) {
                        // we found the current peak, so now we have the iterator before and after
                        ++after;
                        
                        // wrap around if necessary
                        if(after == extrema.end())
                            after = extrema.begin();
                        
                        break;
                    }
                    
                    prev = after;
                }
                
                scalar_t minimum_left = n_max, index_left = std::get<0>(*prev);
                scalar_t minimum_right = n_max, index_right = std::get<0>(*after);
                
                // find boundary to the left (by a higher peak) and right
                scalar_t left_border = 0, right_border = points->size();
                for (auto &range : ranges) {
                    if(range.end > left_border && range.end < peak.position.x) {
                        left_border = range.end;
                        //Print("\tExtending left border to ", left_border);
                    }
                    
                    if(range.start < right_border && range.start > peak.position.x) {
                        right_border = range.start;
                        //Print("\tExtending right border to ", right_border);
                    }
                }
                
                range_t search_range(left_border, right_border);
                //Print("Search_range for ",peak.position.x," is ",search_range.start,"-",search_range.end,". *prev=",*prev," after=",*after);
                
                auto copy_left = prev;
                scalar_t last_y = peak.position.y;
                
                /*if(std::get<1>(*copy_left)) {
                    last_y = points->at(std::get<0>(*copy_left));
                    last_x = std::get<0>(*copy_left);
                }*/
                
                scalar_t offset = 0;
                while (std::get<0>(*copy_left) + offset >= search_range.start) {
                    auto x = std::get<0>(*copy_left);
                    auto y = points->at(x);
                    
                    if(std::get<1>(*copy_left)) {
                        if(y > last_y * 1.05)
                            break;
                        last_y = y;
                    }
                    
                    if(y < minimum_left) {
                        minimum_left = points->at(x);
                        index_left = x;
                    }
                    
                    if(y > peak.max_y_extrema)
                        peak.max_y_extrema = y;
                    
                    if(copy_left == extrema.begin()) {
                        offset = -float(points->size());
                        copy_left = --extrema.end();
                    }
                    if(copy_left == after)
                        break;
                    --copy_left;
                }
                
                offset = 0;
                copy_left = after;
                
                last_y = peak.position.y;
                
                /*if(std::get<1>(*copy_left)) {
                    last_y = points->at(std::get<0>(*copy_left));
                    last_x = std::get<0>(*copy_left);
                }*/
                
                while (std::get<0>(*copy_left) + offset <= search_range.end) {
                    auto x = std::get<0>(*copy_left);
                    auto y = points->at(x);
                    
                    if(std::get<1>(*copy_left)) {
                        if(y > last_y * 1.05)
                            break;
                        last_y = y;
                    }
                    
                    if(y < minimum_right) {
                        minimum_right = y;
                        index_right = x;
                    }
                    
                    if(y > peak.max_y_extrema)
                        peak.max_y_extrema = y;
                    
                    if(++copy_left == extrema.end()) {
                        offset = points->size();
                        copy_left = extrema.begin();
                    }
                    if(copy_left == prev)
                        break;
                }
                
                while(index_left > peak.position.x)
                    index_left -= points->size();
                while(index_right < peak.position.x)
                    index_right += points->size();
                
                peak.width = search_range.length();
                peak.range = range_t(index_left, index_right);
                
                //Print("Avoiding range ", peak.range.start,"-",peak.range.end);
                ranges.push_back(peak.range);
            }
            
            scalar_t y;
            
            std::unordered_map<Peak*, std::vector<range_t>> check_ranges;
            for(auto &peak : *maxima) {
                std::vector<range_t> check{peak.range};
                auto x0 = peak.range.start;
                auto x1 = peak.range.end;
                
                if (x0 < 0) {
                    x0 += points->size();
                    check.front().start = 0;
                    check.push_back(range_t(x0, points->size()-1));
                }
                if (x1 >= points->size()) {
                    x1 -= points->size();
                    check.front().end = points->size()-1;
                    check.push_back(range_t(0, x1));
                }
                
                check_ranges[&peak] = check;
                //peak.split_ranges = check;
            }
            
            /*scalar_t percent;
            
            for (size_t i=0; i<points->size(); ++i) {
                y = (*points)[i] - min_y;
                
                //for (size_t j=0; j<maxima->size(); ++j) {
                //    auto &peak = maxima->at(j);
                for(auto && [peak, check] : check_ranges) {
                    for(auto & range : check) {
                        percent = (i - range.start) / (range.end - range.start + 1);
                        if (percent >= 0 && percent <= 1 && y / (peak->position.y - min_y) >= 0.5) {
                            peak->points.insert(Vec2(i, y));
                            //peak->median_y.addNumber(y);
                            if(y > peak->max_y)
                                peak->max_y = y;
                            //peak->integral += 1; //peak.position.y - min_y;
                        }
                    }
                }
            }*/
            
            for(auto && [peak, check] : check_ranges) {
                for(auto & range : check) {
                    for(auto i : range.iterable()) {
                        y = (*points)[i] - min_y;
                        
                        if(y / (peak->position.y - min_y) >= 0.5) {
                            //if(!contains(peak->points, Vec2(i, y)))
                            /*if(auto it = find_sorted(peak->points, Vec2(i, y), std::less<>{});
                               it == peak->points.end())
                            {
                                insert_sorted(peak->points, Vec2(i, y));
                            } else {
                                Print("Contains ", Vec2(i, y), " already!");
                            }*/
                                peak->points.emplace_back(i, y);
                            //peak->median_y.addNumber(y);
                            if(y > peak->max_y)
                                peak->max_y = y;
                            //peak->integral += 1; //peak.position.y - min_y;
                        }
                    }
                    //percent = (i - range.start) / (range.end - range.start + 1);
                    //if (percent >= 0 && percent <= 1 &&
                }
            }
            
            for(auto &peak : *maxima) {
                auto median = peak.max_y * 0.5;
                auto factor = (mode == FIND_BROAD ? 1 : peak.position.y);
                for(auto &pt : peak.points) {
                    peak.integral += (pt.y - median) * factor;
                }
            }
            
            return { std::move(maxima), std::move(minima) };
        }
        
        template<bool calculate_sum = false, typename T = points_t::element_type>
        std::tuple<scalar_t, std::vector<std::unique_ptr<T>>> _differentiate(const T& points, size_t times) {
            using pointer_t = typename T::value_type *;
            //assert(points);
            if(points.empty())
                return std::make_tuple(scalar_t(0), std::vector<std::unique_ptr<T>>{});
            
            std::vector<std::unique_ptr<T>> result;
            std::vector<pointer_t> out;
            
            out.resize(times);
            for (size_t i=0; i<times; ++i) {
                result.emplace_back(std::make_unique<T>());
                result.back()->resize(points.size());
                out[i] = result.back()->data();
            }
            
            //auto out = result->data();
            auto prev = points.data();
            auto ptr = points.data() + 1;
            auto end = points.data() + points.size();
            
            scalar_t sum(0);
            pointer_t *o;
            auto oend = out.data() + out.size();
            typename T::value_type value;
            
            // out[n] = a[n+1] - a[n]
            for (; ptr != end; ++ptr) {
                value = *ptr - *prev;
                
                o = out.data();
                for(; o != oend; ++(*o), ++o)
                    *(*o) = value;
                
                if constexpr(calculate_sum)
                    sum += prev->x * ptr->y - ptr->x * prev->y;
                
                prev = ptr;
            }
            
            assert(&points.back() == prev);
            assert(&result.front()->back() == out.front());
            
            ptr = points.data();
            
            value = *ptr - *prev;
            o = out.data();
            for(; o != oend; ++(*o), ++o)
                *(*o) = value;
            
            if constexpr(calculate_sum)
                sum += ptr->x * prev->y - prev->x * ptr->y;
            
            return std::make_tuple(sum, std::move(result));
        }
        
        std::vector<scalars_t> differentiate(const scalars_t::element_type& points, size_t times) {
            return std::get<1>(_differentiate<false>(points, times));
        }
        
        std::vector<points_t> differentiate(const points_t::element_type& points, size_t times) {
            return std::get<1>(_differentiate<false>(points, times));
        }
        
        std::tuple<scalar_t, std::vector<points_t>> differentiate_and_test_clockwise(const points_t::element_type& points, size_t times) {
            return _differentiate<true>(points, times);
        }
        
        namespace EFT {
            std::tuple<points_t, scalars_t, scalars_t, scalars_t> dt(const points_t& dxy) {
                if (!dxy || dxy->empty())
                    throw U_EXCEPTION("[periodic::EFT] Cannot work on empty (or null) dxy array.");
                
                auto dt = std::make_unique<scalars_t::element_type>();
                dt->resize(dxy->size() - 1); // -1 because dxy is same size as array
                
                auto cumsum = std::make_unique<scalars_t::element_type>();
                cumsum->resize(dt->size() + 1); // prepend a zero
                
                auto phi = std::make_unique<scalars_t::element_type>();
                phi->resize(cumsum->size());
                
                auto cache_xy_sum = std::make_unique<points_t::element_type>();
                cache_xy_sum->resize(dt->size());
                
                scalar_t sum = 0;
                
                for(size_t i = 0; i<dxy->size() - 1; ++i) {
                    const auto &pt = (*dxy)[i];
                    (*dt)[i] = sqrt(SQR(pt.x) + SQR(pt.y)) + 1e-10;
                    
                    sum += (*dt)[i];
                    (*cumsum)[i+1] = sum;
                    
                    // add a small term so we wont get problems with divide by zero later
                    (*phi)[i+1] = 2 * M_PI * (*cumsum)[i+1];
                    
                    (*cache_xy_sum)[i] = pt / (*dt)[i];
                }
                
                // return cache, phi, t and dt
                return {std::move(cache_xy_sum), std::move(phi), std::move(cumsum), std::move(dt)};
            }
        }
    
        coeff_t eft(const points_t::element_type& points, size_t order, const points_t& _dxy) {
            if(points.empty())
                return nullptr;
            
            points_t tmp;
            const points_t* dxy;
            if(not _dxy) {
                tmp = std::move(differentiate(points, 1).front());
                dxy = &tmp;
            } else {
                dxy = &_dxy;
            }
                
            auto && [cache_xy_sum, phi, cumsum, dt] = EFT::dt(*dxy);
            auto T = cumsum->back(); // period is the circumference
            
            const scalar_t norm_base = T / (2 * SQR(M_PI));
            auto coeffs = std::make_unique<coeff_t::element_type>();
            coeffs->resize(order);
            
            std::vector<Vec2> cossin_phi;
            cossin_phi.resize(phi->size());
            
            for (size_t n = 1; n < order + 1; ++n) {
                auto norm = norm_base / SQR(n);
                point_t cos_n(0), sin_n(0);
                
                // precalculate all cos/sin values
                for (size_t i=0; i<cossin_phi.size(); ++i) {
                    auto phi_n = (*phi)[i] * n / T;
                    cossin_phi[i].x = fast::cos(phi_n);
                    cossin_phi[i].y = fast::sin(phi_n);
                }
                
                for (size_t i=0; i<cossin_phi.size() - 1; ++i) {
                    cos_n += (*cache_xy_sum)[i] * (cossin_phi[i+1].x - cossin_phi[i].x);
                    sin_n += (*cache_xy_sum)[i] * (cossin_phi[i+1].y - cossin_phi[i].y);
                }
                
                cos_n *= norm;
                sin_n *= norm;
                
                (*coeffs)[n-1].x = cos_n.x; // a_n
                (*coeffs)[n-1].y = sin_n.x; // b_n
                
                (*coeffs)[n-1].z = cos_n.y; // c_n
                (*coeffs)[n-1].w = sin_n.y; // d_n
            }
            
            return coeffs;
        }
        
        std::vector<points_t> ieft(const coeff_t::element_type& coeffs, size_t order, size_t n_points, Vec2 offset, bool save_steps, Float2_t scale)
        {
            if(order > coeffs.size())
                throw U_EXCEPTION("Cannot compute order > coeffs.size().");
            
            assert(n_points > 0);
            
            std::vector<points_t> result;
            std::vector<scalar_t> t;
            t.resize(n_points);
            
            std::vector<point_t> pt;
            pt.resize(n_points);
            
            for(size_t i=0; i<t.size(); ++i) {
                t[i] = double(i) / double(t.size()-1) * M_PI * 2.0;
                pt[i] = offset;
            }
            
            for (size_t i=0; i<order; ++i) {
                for (size_t j=0; j<n_points; ++j) {
                    auto ct = fast::cos(t[j] * (i+1));
                    auto st = fast::sin(t[j] * (i+1));
                    
                    pt[j].x += scale * (coeffs[i].x * ct + coeffs[i].y * st);
                    pt[j].y += scale * (coeffs[i].z * ct + coeffs[i].w * st);
                }
                
                if(save_steps)
                    result.emplace_back(std::make_unique<points_t::element_type>(pt.begin(), pt.end()));
            }
            
            if(!save_steps)
                result.emplace_back(std::make_unique<points_t::element_type>(pt.begin(), pt.end()));
            
            return result;
        }
        
        Curve::Curve() {
            
        }
        
        void Curve::make_clockwise() {
            if(!_points)
                throw U_EXCEPTION("[periodic] Have to set points first.");
            if(_is_clockwise)
                return;
            
            std::reverse(_points->begin(), _points->end());
            
            for(auto &d : _derivatives) {
                if(d)
                    d = std::move(differentiate(*_points, 1)[0]);
            }
            
            _is_clockwise = true;
        }
        
        void Curve::set_points(points_t&& points, bool copy) {
            // retain a pointer of the points / replace the old ones
            if(copy)
                _points = std::make_unique<points_t::element_type>(*points);
            else
                _points = std::move(points);
            
            //! calculate som basic properties of the curve
            auto && [sum, der] = _differentiate<true>(*_points, 1);
            _derivatives = std::move(der);
            _is_clockwise = sum >= 0 ? true : false;
        }
    }
}
