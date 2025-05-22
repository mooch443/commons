// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <commons.pc.h>
#include <misc/vec2.h>      // your project’s Vec2  (must have .x and .y)

// ─────────────────────────────────────────────────────────────────────────────
namespace cmn
{
    /**
     * @brief Generic x/y interpolator with compile-time algorithm selection.
     *
     * The interpolator stores a strictly-ascending set of samples (x,y) and
     * supports **nearest**, **linear**, and **natural cubic-spline** lookup.
     *
     * @tparam F  Floating-point type (`float`, `double`, `long double`, …)
     */
    template<std::floating_point F = Float2_t>
    class [[nodiscard]] SampleInterpolator
    {
    public:
        /**
         * @brief Available interpolation algorithms.
         *
         * Selecting an algorithm at compile time lets the optimiser remove the
         * unused code path entirely.
         */
        enum class Method : std::uint8_t { nearest, linear, cubic };

        using value_type     = F;
        using point_type     = cmn::Vec2;                 ///< external 2-D point
        using container_type = std::vector<point_type>;
        using size_type      = std::size_t;

        /// @name Construction / sample management
        /// @{
        SampleInterpolator() = default;

        /**
         * @brief Construct from any range of `Vec2`-like points.
         * @throws std::invalid_argument on empty set or duplicate x values.
         */
        template<std::ranges::input_range R>
        explicit SampleInterpolator(R&& rng) { set_samples(rng); }

        /** Replace all samples.  Throws on empty set or duplicate x. */
        template<std::ranges::input_range R>
        void set_samples(R&& rng)
        {
            container_type v(std::ranges::begin(rng), std::ranges::end(rng));
            if (v.empty()) throw std::invalid_argument("empty sample set");

            std::ranges::sort(v, {}, &point_type::x);
            if (std::adjacent_find(v.begin(), v.end(),
                                   [](auto const& a, auto const& b)
                                   { return a.x == b.x; }) != v.end())
                throw std::invalid_argument("duplicate x coordinates");

            pts_       = std::move(v);
            y2_ready_  = false;
        }

        /** Insert one sample keeping x-order.  Throws on duplicate x. */
        void add_sample(point_type p)
        {
            auto it = std::ranges::lower_bound(pts_, p.x, {}, &point_type::x);
            if (it != pts_.end() && it->x == p.x)
                throw std::invalid_argument("duplicate x coordinate");
            pts_.insert(it, p);
            y2_ready_ = false;
        }
        /// @}

        /**
         * @brief Query the interpolated (or extrapolated) y value at **x**.
         *
         * @tparam M  Interpolation algorithm (compile-time constant).
         * @param  x  Position on the x-axis (may lie outside the sample range).
         * @return    Interpolated / extrapolated y value.
         *
         * Example:
         * @code
         * double y = interp.template operator()<SampleInterpolator<double>::Method::cubic>(2.3);
         * @endcode
         */
        template<Method M = Method::linear>
        [[nodiscard]] constexpr value_type operator()(value_type x) const noexcept
        {
            static_assert(M == Method::nearest ||
                          M == Method::linear  ||
                          M == Method::cubic,   "unsupported Method");

            assert(!pts_.empty() && "Interpolator has no samples");

            if constexpr (M == Method::nearest) return nearest_(x);
            if constexpr (M == Method::linear ) return linear_ (x);
            /* M == cubic */                    return cubic_  (x);
        }

        /** @return `true` if the interpolator holds no samples. */
        [[nodiscard]] constexpr bool empty() const noexcept { return pts_.empty(); }

        /** @return The current number of samples. */
        [[nodiscard]] constexpr size_type size() const noexcept { return pts_.size(); }

    private:
        // ── helpers ──────────────────────────────────────────────────────────
        [[nodiscard]] constexpr value_type nearest_(value_type x) const noexcept
        {
            auto it = std::ranges::lower_bound(pts_, x, {}, &point_type::x);
            if (it == pts_.begin())  return it->y;
            if (it == pts_.end())    return pts_.back().y;
            auto idx = static_cast<size_type>(std::distance(pts_.begin(), it));
            auto const& L = pts_[idx - 1];
            auto const& R = pts_[idx];
            return (x - L.x < R.x - x) ? L.y : R.y;
        }

        [[nodiscard]] constexpr value_type linear_(value_type x) const noexcept
        {
            auto const N = pts_.size();

            if (x <= pts_.front().x) return extrapolate_(pts_[0],   pts_[1],   x);
            if (x >= pts_.back().x)  return extrapolate_(pts_[N-2], pts_[N-1], x);

            auto itR = std::ranges::upper_bound(pts_, x, {}, &point_type::x);
            auto idx = static_cast<size_type>(std::distance(pts_.begin(), itR)) - 1;
            return interpolate_(pts_[idx], pts_[idx+1], x);
        }

        [[nodiscard]] value_type cubic_(value_type x) const noexcept
        {
            if (pts_.size() < 3) return linear_(x);
            if (!y2_ready_) build_spline_();

            size_type lo = 0, hi = pts_.size() - 1;
            while (hi - lo > 1)
            {
                size_type mid = (hi + lo) >> 1;
                (pts_[mid].x > x ? hi : lo) = mid;
            }

            value_type h = pts_[hi].x - pts_[lo].x;
            value_type a = (pts_[hi].x - x) / h;
            value_type b = (x - pts_[lo].x) / h;

            return   a * pts_[lo].y + b * pts_[hi].y
                   + ((a*a*a - a) * y2_[lo] + (b*b*b - b) * y2_[hi])
                     * (h * h) / value_type{6};
        }

        // natural cubic spline ─ second-derivative pre-compute (O(N))
        void build_spline_() const
        {
            auto const n = pts_.size();
            y2_.assign(n, value_type{});
            std::vector<value_type> u(n - 1);

            for (size_type i = 1; i < n - 1; ++i)
            {
                value_type sig = (pts_[i].x - pts_[i-1].x)
                               / (pts_[i+1].x - pts_[i-1].x);
                value_type p   = sig * y2_[i-1] + value_type{2};
                y2_[i] = (sig - value_type{1}) / p;

                value_type d =  (pts_[i+1].y - pts_[i].y)
                               / (pts_[i+1].x - pts_[i].x)
                             - (pts_[i].y   - pts_[i-1].y)
                               / (pts_[i].x   - pts_[i-1].x);
                u[i] = (value_type{6} * d / (pts_[i+1].x - pts_[i-1].x)
                        - sig * u[i-1]) / p;
            }
            for (size_type k = n - 2; k-- > 0; )
                y2_[k] = y2_[k] * y2_[k+1] + u[k];

            y2_ready_ = true;
        }

        // branch-free segment helpers
        [[nodiscard]] static constexpr value_type interpolate_(
            point_type const& p0, point_type const& p1, value_type x) noexcept
        {
            value_type t = (x - p0.x) / (p1.x - p0.x);
            return std::fma(p1.y - p0.y, t, p0.y);          // y0 + (y1-y0)*t
        }

        [[nodiscard]] static constexpr value_type extrapolate_(
            point_type const& p0, point_type const& p1, value_type x) noexcept
        {
            value_type slope = (p1.y - p0.y) / (p1.x - p0.x);
            return std::fma(slope, (x - p0.x), p0.y);
        }

        // data
        container_type         pts_;
        mutable std::vector<F> y2_;
        mutable bool           y2_ready_{false};
    };

} // namespace utils
