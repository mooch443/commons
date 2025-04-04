#ifndef VIDEO_TYPES_H
#define VIDEO_TYPES_H

#ifndef WIN32
    #if __cplusplus <= 199711L
      #error This library needs at least a C++11 compliant compiler
    #endif

    #define TREX_EXPORT
    #define EXPIMP_TEMPLATE

#else
    #ifdef TREX_EXPORTS
        #define TREX_EXPORT __declspec(dllexport)
        #define EXPIMP_TEMPLATE
    #else
        #define TREX_EXPORT __declspec(dllimport)
        #define EXPIMP_TEMPLATE extern
    #endif
#endif


#include <misc/metastring.h>
#include <misc/utilsexception.h>
#include <misc/format.h>
#include <misc/checked_casts.h>
#include <misc/EnumClass.h>
#include <misc/custom_exceptions.h>
#include <misc/detail.h>
#include <misc/vec2.h>

#ifdef _MSC_VER
#include <intrin.h>

DWORD __inline __builtin_ctz32(uint32_t value)
{
    DWORD trailing_zero = 0;
    _BitScanForward(&trailing_zero, value);
    return trailing_zero;
}
#else
inline int __builtin_ctz32(uint32_t value) {
    return __builtin_ctz(value);
}
#endif

/**
 * ======================
 * THREAD-SAFE methods
 * ======================
 */

namespace cmn {
namespace blob {
using lines_t = std::vector<HorizontalLine>;
using line_ptr_t = std::unique_ptr<lines_t>;
using pixel_ptr_t = std::unique_ptr<std::vector<uchar>>;

enum class Focus {
    Center,
    Left,
    Right
};

// Linear Weighting
template<Focus Dir>
struct LinearTemporalWeighting {
    static float calculate_weight(size_t current_index, size_t array_length) {
        float center = (Dir == Focus::Center) ? array_length / 2.0f :
                       (Dir == Focus::Left)  ? 0.0f :
                       array_length;
        return 1.0f - std::abs(center - static_cast<float>(current_index)) / array_length;
    }
};

// Exponential Weighting
template<Focus Dir>
struct ExponentialTemporalWeighting {
    static float calculate_weight(size_t current_index, size_t array_length) {
        float center = (Dir == Focus::Center) ? array_length / 2.0f :
                       (Dir == Focus::Left)  ? 0.0f :
                       array_length;
        return std::exp(-std::abs(center - static_cast<float>(current_index)) / array_length);
    }
};

// Gaussian Weighting
template<Focus Dir>
struct GaussianTemporalWeighting {
    static float calculate_weight(size_t current_index, size_t array_length) {
        float center = (Dir == Focus::Center) ? array_length / 2.0f :
                       (Dir == Focus::Left)  ? 0.0f :
                       array_length;
        float sigma = array_length / 6.0f;
        float diff = center - static_cast<float>(current_index);
        return std::exp(-0.5f * diff * diff / (sigma * sigma));
    }
};

struct Pose {
    struct Point {
        uint16_t x, y;
        
        constexpr Point(const cmn::Vec2& v)
            : x(cmn::narrow_cast<uint16_t>(v.x)),
              y(cmn::narrow_cast<uint16_t>(v.y))
        {
            assert(v.x >= 0 && v.y >= 0);
        }
        constexpr Point(uint16_t x, uint16_t y) noexcept : x(x), y(y) {}
        constexpr Point() noexcept = default;
        constexpr Point(const Point&) noexcept = default;
        constexpr Point& operator= (const Point&) noexcept = default;
        constexpr Point& operator= (Point&&) noexcept = default;

        constexpr bool operator==(const Point&) const noexcept = default;
        constexpr bool operator!=(const Point&) const noexcept = default;
        
        bool valid() const noexcept {
            return x > 0 || y > 0;
        }

        constexpr operator Vec2() const noexcept {
            return Vec2(x, y);
        }
        
        static Point fromStr(const std::string& str) {
            return Point(cmn::Meta::fromStr<Vec2>(str));
        }
        static std::string class_name() {
            return "Pose::Point";
        }
        std::string toStr() const {
            return "["+cmn::Meta::toStr(x)+","+cmn::Meta::toStr(y)+"]";
        }
        glz::json_t to_json() const {
            // output as [x, y]
            return {x, y};
        }
        
        constexpr static Point invalid() noexcept { return Point{0, 0}; }
    };
    
    struct Bone {
        Point A, B;
        Bone(const std::vector<Point>& points, size_t index)
            :  A(points.at(index)), B(points.at(index + 1 >= points.size() ? 0u : (index + 1)))
        { }
        
        [[nodiscard]] float length() const noexcept { return euclidean_distance(A, B); }
    };

    struct Skeleton {
    public:
        struct Connection {
            uint8_t from{0}, to{0};
            std::string name;
            std::string toStr() const;

            auto operator<=>(const Connection&) const = default;
            constexpr bool operator==(const Connection& other) const noexcept = default;
            constexpr bool operator!=(const Connection& other) const noexcept = default;

            static std::string class_name() noexcept { return "Connection"; }
            static Connection fromStr(const std::string&);
            glz::json_t to_json() const;
        };
        
    protected:
        static std::mutex _mutex;
        static std::unordered_map<std::string, Skeleton> _registered;
        
        GETTER(std::string, name);
        GETTER(std::vector<Connection>, connections);
        
    public:
        Skeleton() = default;
        Skeleton(std::string name, std::vector<Connection>&& connections)
            : _name(name),
              _connections(std::move(connections))
        {}

        std::strong_ordering operator<=>(const Skeleton& other) const {
            // first compare the name, then the connections (manually)
            auto cmp = _name <=> other._name;
            if(cmp != 0)
                return cmp;
            for(size_t i = 0; i < _connections.size(); ++i) {
                cmp = _connections[i] <=> other._connections[i];
                if(cmp != 0)
                    return cmp;
            }
            return std::strong_ordering::equivalent;
        }
        bool operator==(const Skeleton& other) const noexcept = default;
        bool operator!=(const Skeleton& other) const noexcept = default;

        static Skeleton fromStr(const std::string&);
        std::set<uint8_t> keypoint_indexes() const;
        bool empty() const;
        std::string toStr() const;
        glz::json_t to_json() const;
        static std::string class_name() noexcept { return "Skeleton"; }
        static void add(Skeleton&&);
        static Skeleton get(const std::string&);
        static bool exists(const std::string& name);
    };
    
    //! coordinates of the bones
    std::vector<Point> points;
    
    [[nodiscard]] Bone bone(size_t index) const {
        if(index >= points.size())
            throw std::out_of_range("Index "+cmn::Meta::toStr(index)+" out of range for Pose with "+cmn::Meta::toStr(size())+" points.");
        return Bone(points, index);
    }
    
    [[nodiscard]] const Point& point(size_t index) const {
        return points.at(index);
    }
    
    [[nodiscard]] size_t size() const noexcept { return points.size(); }
    [[nodiscard]] bool empty() const noexcept { return points.empty(); }

    template<typename WeightPolicy, typename T>
        requires requires (T t) { { t(std::size_t{}) } -> std::convertible_to<const Pose&>; }
    static auto mean(T&& values, std::size_t num, const std::vector<size_t>& ignore_indexes = {}) {
        if(num == 0) {
            throw std::out_of_range("Number of poses must be greater than zero.");
        }

        std::vector<Vec2> relativePoints(values(0).points.size());
        std::vector<float> weights(relativePoints.size(), 0.0f);

        Vec2 referencePointSum(0, 0);
        float referenceWeightSum = 0.0f;

        for(size_t i = 0; i < num; ++i) {
            const Pose& pose = values(i);
            if(pose.empty())
                continue;
            
            double samples = 0;
            Vec2 referencePoint;// = pose.points[referenceJointIndex];
            for(auto &pt : pose.points) {
                if(pt.valid()) {
                    referencePoint += Vec2(pt);
                    samples++;
                }
            }
            if(samples > 0)
                referencePoint /= samples;
            else
                continue;
            
            float weight = WeightPolicy::calculate_weight(i, num);

            for(size_t j = 0; j < pose.points.size(); ++j) {
                if(pose.points[j].valid()) {
                    Vec2 relativePos = Vec2(pose.points[j]) - referencePoint;
                    relativePoints[j] += relativePos * weight;
                    weights[j] += weight;
                }
            }

            referencePointSum += referencePoint * weight;
            referenceWeightSum += weight;
        }

        if(referenceWeightSum == 0.0f) {
            return Pose{};
            //throw std::runtime_error("Reference weight sum cannot be zero.");
        }

        Vec2 averageReferencePoint = referencePointSum / referenceWeightSum;
        
        Pose pose;
        pose.points.resize(relativePoints.size());

        for(size_t i = 0; i < relativePoints.size(); ++i) {
            if(weights[i] == 0.0f || contains(ignore_indexes, i)) {
                pose.points[i] = Point::invalid();
            } else {
                Vec2 meanRelativePos = relativePoints[i] / weights[i];
                pose.points[i] = Point((averageReferencePoint + meanRelativePos).clip(0));
            }
        }
        
        return pose;
    }

    std::string toStr() const;
    static std::string class_name();
    static Pose fromStr(const std::string&);

    constexpr bool operator==(const Pose& other) const noexcept {
        return points == other.points;
    }
    glz::json_t to_json() const;
    
    /**
     * Serializes the Pose object to a byte array.
     * @return A vector containing the serialized byte data.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserializes a Pose object from a byte array.
     * @param buffer A vector containing the serialized byte data.
     * @throws std::invalid_argument if the buffer is malformed.
     */
    static Pose deserialize(const std::vector<uint8_t>& buffer);
};

struct SegmentedOutlines {
    struct Outline {
        std::vector<int32_t> _points; // has to be divisible by 2 (x,y)
        //bool valid() const { return not _points.empty(); }
        size_t size() const {
            return _points.size() / 2u; // assume divisible by 2
        }
        Vec2 operator[](size_t index) const {
            return Vec2(_points[index * 2u], _points[index * 2u + 1]);
        }
        operator std::vector<Vec2>() const {
            const size_t N = size();
            std::vector<Vec2> pts;
            pts.resize(N);
            for(size_t index = 0; index < N; ++index)
                pts[index] = (*this)[index];
            return pts;
        }
        
        static Outline from_vectors(const std::vector<Vec2>& pts) {
            Outline outline;
            outline._points.resize(pts.size() * 2);
            for(size_t i=0, N = pts.size(); i<N; ++i) {
                auto& pt = pts[i];
                outline._points[i * 2u] = (int32_t(pt.x));
                outline._points[i * 2u + 1] = (int32_t(pt.y));
            }
            return outline;
        }
        
        constexpr bool operator==(const Outline&) const noexcept = default;
    };
    
    std::optional<Outline> original_outline;
    std::vector<Outline> lines;
    
    bool has_original_outline() const { return original_outline.has_value(); }
    bool has_holes() const { return not lines.empty(); }
    
    void add(Outline&& outline) {
        lines.emplace_back(std::move(outline));
    }
    
    void set_original(const std::vector<Vec2>& pts) {
        if(original_outline.has_value())
            throw std::runtime_error("Outline is already set.");
        original_outline = Outline::from_vectors(pts);
    }
    
    void add(const std::vector<Vec2>& pts) {
        lines.emplace_back(Outline::from_vectors(pts));
    }
    
    constexpr bool operator==(const SegmentedOutlines&) const noexcept = default;
};

struct Prediction {
    uint8_t clid{255u}; // up to 254 classes
    uint8_t p{0u}; // [0,255] -> [0,1]
    blob::Pose pose;
    blob::SegmentedOutlines outlines;
    
    bool valid() const { return clid < 255u; }
    std::string toStr() const;
    static std::string class_name() { return "Prediction"; }
    constexpr float probability() const { return float(p) / 255.f; }
    
    constexpr bool operator==(const Prediction&) const noexcept = default;
};

struct Pair {
    blob::line_ptr_t lines;
    blob::pixel_ptr_t pixels;
    uint8_t extra_flags;
    Prediction pred;
    
    Pair() = default;
    Pair(Pair&&) = default;
    Pair(line_ptr_t&& lines, pixel_ptr_t&& pixels, uint8_t extra_flags = 0, Prediction&& pred = {});
    Pair& operator=(Pair&&) = default;
};
}

typedef std::vector<blob::Pair> blobs_t;
constexpr int CV_MAX_THICKNESS = 32767;
}


#define DEBUG_CV(COMMAND) try { COMMAND; } catch(const cv::Exception& e) { FormatExcept("OpenCV Exception ('", __FILE__ ,"':", __LINE__ ,"): ", #COMMAND ,"\n", e.what()); }

namespace cmn::tf {
    void imshow(const std::string& name, const cv::Mat& mat, std::string label = "");
    void imshow(const std::string& name, const gpuMat& mat, std::string label = "");
    void destroyAllWindows();
    void show();
    void waitKey(std::string name);
}

namespace track {
    
}

#endif
