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

#include <misc/detail.h>

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

#include <misc/vec2.h>

/**
 * ======================
 * THREAD-SAFE methods
 * ======================
 */

namespace cmn {
namespace blob {
using line_ptr_t = std::unique_ptr<std::vector<HorizontalLine>>;
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
            : x(narrow_cast<uint16_t>(v.x)),
              y(narrow_cast<uint16_t>(v.y))
        {
            assert(v.x >= 0 && v.y >= 0);
        }
        constexpr Point(uint16_t x, uint16_t y) noexcept : x(x), y(y) {}
        constexpr Point() noexcept = default;
        constexpr Point(const Point&) noexcept = default;
        constexpr Point& operator= (const Point&) noexcept = default;
        constexpr Point& operator= (Point&&) noexcept = default;

        constexpr operator Vec2() const noexcept {
            return Vec2(x, y);
        }
        
        static Point fromStr(const std::string& str) {
            return Point(Meta::fromStr<Vec2>(str));
        }
        static std::string class_name() {
            return "Pose::Point";
        }
        std::string toStr() const {
            return "["+Meta::toStr(x)+","+Meta::toStr(y)+"]";
        }
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
            bool operator==(const Connection& other) const noexcept = default;
            bool operator!=(const Connection& other) const noexcept = default;

            static std::string class_name() noexcept { return "Connection"; }
            static Connection fromStr(const std::string&);
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
        std::string toStr() const;
        static std::string class_name() noexcept { return "Skeleton"; }
        static void add(Skeleton&&);
        static Skeleton get(const std::string&);
        static bool exists(const std::string& name);
    };
    
    //! coordinates of the bones
    std::vector<Point> points;
    
    [[nodiscard]] Bone bone(size_t index) const {
        if(index >= points.size())
            throw std::out_of_range("Index "+Meta::toStr(index)+" out of range for Pose with "+Meta::toStr(size())+" points.");
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
                if(pt.x > 0 || pt.y > 0) {
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
                if(pose.points[j].x > 0 || pose.points[j].y > 0) {
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
                pose.points[i] = Point(0, 0);
            } else {
                Vec2 meanRelativePos = relativePoints[i] / weights[i];
                pose.points[i] = Point((averageReferencePoint + meanRelativePos).clip(0));
            }
        }
        
        return pose;
    }

    
    /**
     * Serializes the Pose object to a byte array.
     * @return A vector containing the serialized byte data.
     */
    std::vector<uint8_t> serialize() const {
        // Reserve space for the number of points (size_t) and each point (2 * uint16_t)
        std::vector<uint8_t> buffer(4 * points.size());

        // Serialize the number of points first, to simplify deserialization
        size_t offset = 0;
        
        // Serialize each point
        for (const auto& point : points) {
            *reinterpret_cast<uint16_t*>(&buffer[offset]) = point.x;
            offset += sizeof(uint16_t);
            *reinterpret_cast<uint16_t*>(&buffer[offset]) = point.y;
            offset += sizeof(uint16_t);
        }
        
        return buffer;
    }

    /**
     * Deserializes a Pose object from a byte array.
     * @param buffer A vector containing the serialized byte data.
     * @throws std::invalid_argument if the buffer is malformed.
     */
    static Pose deserialize(const std::vector<uint8_t>& buffer) {
        // Read the number of points
        size_t num_points = buffer.size() / 4u;

        Pose pose;
        // Clear existing points
        pose.points.clear();
        pose.points.reserve(num_points);

        // Deserialize each point
        size_t offset = 0;
        for (size_t i = 0; i < num_points; ++i) {
            uint16_t x = *reinterpret_cast<const uint16_t*>(&buffer[offset]);
            offset += sizeof(uint16_t);
            uint16_t y = *reinterpret_cast<const uint16_t*>(&buffer[offset]);
            offset += sizeof(uint16_t);
            pose.points.emplace_back(Point{x, y});
        }
        
        return pose;
    }
};

struct Prediction {
    uint8_t clid{255u}; // up to 254 classes
    uint8_t p{0u}; // [0,255] -> [0,1]
    blob::Pose pose;
    
    bool valid() const { return clid < 255u; }
    std::string toStr() const;
    static std::string class_name() { return "Prediction"; }
    constexpr float probability() const { return float(p) / 255.f; }
};

struct Pair {
    blob::line_ptr_t lines;
    blob::pixel_ptr_t pixels;
    uint8_t extra_flags;
    Prediction pred;
    
    Pair() = default;
    Pair(line_ptr_t&& lines, pixel_ptr_t&& pixels, uint8_t extra_flags = 0, Prediction&& pred = {})
        : lines(std::move(lines)),
          pixels(std::move(pixels)),
          extra_flags(extra_flags),
          pred(std::move(pred))
    {}
    /*Pair& operator=(Pair&& other) {
        if(!lines) {
            lines = std::move(other.lines);
            pixels = std::move(other.pixels);
        } else {
            *lines = std::move(*other.lines);
            *pixels = std::move(*other.pixels);
        }
        return *this;
    }*/
};
}

typedef std::vector<blob::Pair> blobs_t;
constexpr int CV_MAX_THICKNESS = 32767;
}


#define DEBUG_CV(COMMAND) try { COMMAND; } catch(const cv::Exception& e) { FormatExcept("OpenCV Exception ('", __FILE__ ,"':", __LINE__ ,"): ", #COMMAND ,"\n", e.what()); }

namespace tf {
    void imshow(const std::string& name, const cv::Mat& mat, std::string label = "");
    void show();
    void waitKey(std::string name);
}

namespace gui {
    using namespace cmn;
}

namespace track {
    using namespace cmn;
}

#endif
