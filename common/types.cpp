#include "types.h"
#include <png.h>
#include <misc/Image.h>
#include <misc/GlobalSettings.h>
#include <misc/PVBlob.h>

namespace cmn {
IMPLEMENT(blob::Pose::Skeleton::_mutex);
IMPLEMENT(blob::Pose::Skeleton::_registered);

blob::Pair::Pair(line_ptr_t&& lines, pixel_ptr_t&& pixels, uint8_t extra_flags, Prediction&& pred)
    : lines(std::move(lines)),
      pixels(std::move(pixels)),
      extra_flags(extra_flags),
      pred(std::move(pred))
{
    assert(not this->pixels || this->pixels->size() % (pv::Blob::is_flag(extra_flags, pv::Blob::Flags::is_rgb) ? 3 : 1) == 0);
}

std::string blob::Prediction::toStr() const {
    std::map<uint16_t, std::string> detect_classes;
    if(GlobalSettings::has("detect_classes")) {
        detect_classes = SETTING(detect_classes).value<std::map<uint16_t, std::string>>();
    }
    
    if(valid()) {
        auto it = detect_classes.find(clid);
        return (it == detect_classes.end() ? "unknown<"+Meta::toStr(clid)+">" : it->second) + "["+dec<2>( p / 255_F * 100_F).toStr()+"%]";
    }
    return "null";
}

std::string blob::Pose::Skeleton::toStr() const {
    return "[" + Meta::toStr(name()) + "," + Meta::toStr(_connections) + "]";
}

glz::json_t blob::Pose::Skeleton::to_json() const {
    return glz::json_t{ name(), cvt2json(connections()) };
}

std::string blob::Pose::Skeleton::Connection::toStr() const {
    return "["+Meta::toStr(from) + "," + Meta::toStr(to)+"]";
}

glz::json_t blob::Pose::Skeleton::Connection::to_json() const {
    return glz::json_t{from, to};
}

blob::Pose::Skeleton::Connection blob::Pose::Skeleton::Connection::fromStr(const std::string &str) {
    auto parts = util::parse_array_parts(util::truncate(str));
    if(parts.size() != 2 && parts.size() != 3)
        throw InvalidArgumentException("Invalid connection: ", str);
    std::string name = Meta::toStr<std::string>("<unknown>");
    if(parts.size() > 2) {
        name = parts.at(2);
    }
    return Connection{
        .from = Meta::fromStr<uint8_t>(parts.at(0)),
        .to = Meta::fromStr<uint8_t>(parts.at(1)),
        .name = ""//Meta::fromStr<std::string>(name)
    };
}

blob::Pose::Skeleton blob::Pose::Skeleton::fromStr(const std::string &str) {
    auto outer = util::parse_array_parts(util::truncate(str));
    if(outer.size() < 2)
        throw InvalidArgumentException("Invalid skeleton string: ", str);

    auto array = Meta::fromStr<std::vector<blob::Pose::Skeleton::Connection>>(outer.at(1));
    return Skeleton(Meta::fromStr<std::string>(outer.at(0)), std::move(array));
}

void blob::Pose::Skeleton::add(Skeleton && s) {
    std::unique_lock guard(_mutex);
    auto name = s.name();
    _registered[name] = std::move(s);
}

blob::Pose::Skeleton blob::Pose::Skeleton::get(const std::string &name) {
    std::unique_lock guard(_mutex);
    return _registered.at(name);
}

bool blob::Pose::Skeleton::exists(const std::string& name) {
    std::unique_lock guard(_mutex);
    return _registered.find(name) not_eq _registered.end();
}

bool blob::Pose::Skeleton::empty() const {
    return _connections.empty();
}

std::set<uint8_t> blob::Pose::Skeleton::keypoint_indexes() const
{
    std::set<uint8_t> indexes;
    for(auto &[from, to, _] : _connections) {
        indexes.insert(from);
        indexes.insert(to);
    }
    return indexes;
}

blob::Pose blob::Pose::fromStr(const std::string &str) {
    auto points = Meta::fromStr<std::vector<Point>>(str);
    return blob::Pose{std::move(points)};
}

std::string blob::Pose::toStr() const {
    std::string str = "[";
    for(size_t i = 0; i < points.size(); ++i) {
        str += points[i].toStr();
        if(i + 1 < points.size())
            str += ",";
    }
    return str + "]";
}

std::string blob::Pose::class_name() {
    return "Pose";
}

glz::json_t blob::Pose::to_json() const {
    std::vector<glz::json_t> j;
    for(size_t i = 0; i < points.size(); ++i) {
        j.push_back(points[i].to_json());
    }
    return j;
}

/**
 * Serializes the Pose object to a byte array.
 * @return A vector containing the serialized byte data.
 */
std::vector<uint8_t> blob::Pose::serialize() const {
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
blob::Pose blob::Pose::deserialize(const std::vector<uint8_t>& buffer) {
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

}

namespace cmn::tf {
    
    class ThreadSafety {
        struct DestroyItem {};
        struct WaitItem {};
        struct DisplayItem {
			std::string name;
			cmn::Image::Ptr image;
        };

        static std::mutex _opencv_lock;
        static std::queue<std::variant<DestroyItem, DisplayItem, WaitItem>> images;
        
    public:
        struct LockGuard {
            std::unique_lock<std::mutex> _lock;
            
            LockGuard() : _lock(ThreadSafety::_opencv_lock)
            { }
        };
        
        static void add(std::string name, const cv::Mat& matrix, std::string label);
        static void show();
        static void waitKey(std::string name);
        static void destroyAllWindows();
        
        ThreadSafety() {}
        ~ThreadSafety();
    };
    
    std::mutex ThreadSafety::_opencv_lock;
    decltype(ThreadSafety::images) ThreadSafety::images;
    
    void imshow(const std::string& name, const cv::Mat& mat, std::string label) {
        ThreadSafety::add(name, mat, label);
    }
    void destroyAllWindows() {
        ThreadSafety::destroyAllWindows();
    }

    void imshow(const std::string& name, const gpuMat& mat, std::string label) {
        cv::Mat cpu;
        mat.copyTo(cpu);
        ThreadSafety::add(name, cpu, label);
    }
    
    void show() {
        ThreadSafety::show();
    }
    
    void waitKey(std::string name) {
        ThreadSafety::waitKey(name);
    }
    
    ThreadSafety::~ThreadSafety() {
        LockGuard guard;
        while(!images.empty()) {
            auto &&img = std::move(images.front());
            images.pop();

			if (std::holds_alternative<DestroyItem>(img)) {
				// Handle DestroyItem
                
			} else if (std::holds_alternative<DisplayItem>(img)) {
                /// delete image
                
            } else if(std::holds_alternative<WaitItem>(img)) {
                /// nothing
            } else {
                /// unknown item type
            }
        }
    }

    void ThreadSafety::destroyAllWindows() {
        LockGuard guard;
		images.push(DestroyItem{});
    }
    
    void ThreadSafety::add(std::string name, const cv::Mat& matrix, std::string label) {
        if(matrix.empty())
            throw InvalidArgumentException("Matrix ", name, " with label ",label," was empty.");
        
		LockGuard guard;

		assert(matrix.isContinuous());
		assert(matrix.type() == CV_8UC(matrix.channels()));
		
		auto ptr = Image::Make(matrix);
		images.push(DisplayItem{ name , std::move(ptr) });
        
        if(!label.empty()) {
            cv::Mat mat = ptr->get();
            cv::putText(mat, label, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 0.8, cv::Scalar(255, 255, 255));
        }
	}

	void ThreadSafety::show() {
		std::unique_lock<std::mutex> lock(_opencv_lock);
        std::map<std::string, Image::Ptr> display_map;
        bool destroy_windows = false;
        bool waitKey = false;
        
        static const bool nowindow = GlobalSettings::map().has("nowindow") ? SETTING(nowindow).value<bool>() : false;

		while(!images.empty()) {
			auto p = std::move(images.front());
			images.pop();

            if (std::holds_alternative<DisplayItem>(p)) {
                auto& [name, ptr] = std::get<DisplayItem>(p);
                display_map[name] = std::move(ptr);
			}
			else if (std::holds_alternative<DestroyItem>(p)) {
                destroy_windows = true;
                
            } else if(std::holds_alternative<WaitItem>(p)) {
                waitKey = true;
                
            } else {
                throw U_EXCEPTION("[tf] Unknown action type.");
            }
		}

        if (destroy_windows) {
            cv::destroyAllWindows();
        }
        
        if(not nowindow) {
            for(auto &p: display_map) {
                cv::Mat mat = p.second->get();
#ifdef __linux__
                resize_image(mat, 1.5, cv::INTER_AREA);
#endif
                cv::imshow(p.first, mat);
            }
        }
        
        if(not waitKey && not nowindow && not display_map.empty()) {
            lock.unlock();
#if !defined(__EMSCRIPTEN__)
            cv::waitKey(1);
#endif
            lock.lock();
        }
        
        if(waitKey && not nowindow) {
            lock.unlock();
#if !defined(__EMSCRIPTEN__)
            cv::waitKey();
#endif
            lock.lock();
        }
	}
    
    void ThreadSafety::waitKey(std::string name) {
        LockGuard guard;
        images.push(DisplayItem{ name, nullptr });
    }
}
