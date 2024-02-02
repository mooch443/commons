#include "types.h"
#include <png.h>
#include <misc/Image.h>
#include <misc/GlobalSettings.h>

namespace cmn {
IMPLEMENT(blob::Pose::Skeleton::_mutex);
IMPLEMENT(blob::Pose::Skeleton::_registered);

std::string blob::Prediction::toStr() const {
    std::vector<std::string> meta_classes;
    if(GlobalSettings::has("meta_classes")) {
        meta_classes = SETTING(meta_classes).value<std::vector<std::string>>();
    }
    
    if(valid())
        return (clid < meta_classes.size() ? meta_classes.at(clid) : "unknown<"+Meta::toStr(clid)+">")+"["+dec<2>( p / 255.f * 100.f).toStr()+"%]";
    return "pred<null>";
}

std::string blob::Pose::Skeleton::toStr() const {
    return "[" + Meta::toStr(name()) + "," + Meta::toStr(_connections) + "]";
}

nlohmann::json blob::Pose::Skeleton::to_json() const {
    auto a = nlohmann::json::array();
    a.push_back(name());
    a.push_back(cvt2json(connections()));
    return a;
}

std::string blob::Pose::Skeleton::Connection::toStr() const {
    return "["+Meta::toStr(from) + "," + Meta::toStr(to)+"]";
}

nlohmann::json blob::Pose::Skeleton::Connection::to_json() const {
    auto a = nlohmann::json::array();
    a.push_back(from);
    a.push_back(to);
    //a.push_back(name);
    return a;
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
        .to = Meta::fromStr<uint8_t>(parts.at(1))//,
        //.name = Meta::fromStr<std::string>(name)
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
}

namespace tf {
    using namespace cmn;
    
    class ThreadSafety {
        static std::mutex _opencv_lock;
        static std::queue<std::pair<std::string, cmn::Image*>> images;
        static std::map<std::string, std::queue<cmn::Image*>> waiting;
        
    public:
        struct LockGuard {
            std::unique_lock<std::mutex> _lock;
            
            LockGuard() : _lock(ThreadSafety::_opencv_lock)
            { }
        };
        
        static void add(std::string name, const cv::Mat& matrix, std::string label);
        static void show();
        static void waitKey(std::string name);
        
        ThreadSafety() {}
        ~ThreadSafety();
    };
    
    decltype(ThreadSafety::waiting) ThreadSafety::waiting;
    std::mutex ThreadSafety::_opencv_lock;
    decltype(ThreadSafety::images) ThreadSafety::images;
    
    void imshow(const std::string& name, const cv::Mat& mat, std::string label) {
        ThreadSafety::add(name, mat, label);
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
            auto &img = images.front();
            delete img.second;
            images.pop();
        }
    }
    
    void ThreadSafety::add(std::string name, const cv::Mat& matrix, std::string label) {
        if(matrix.empty())
            throw InvalidArgumentException("Matrix ", name, " with label ",label," was empty.");
        
		LockGuard guard;

		assert(matrix.isContinuous());
		assert(matrix.type() == CV_8UC(matrix.channels()));
		
		Image *ptr = new Image(matrix);
		images.push({ name , ptr });
        
        if(!label.empty()) {
            cv::Mat mat = ptr->get();
            cv::putText(mat, label, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 0.8, cv::Scalar(255, 255, 255));
        }
	}

	void ThreadSafety::show() {
		std::unique_lock<std::mutex> lock(_opencv_lock);
        std::map<std::string, Image*> display_map;
        
        static const bool nowindow = GlobalSettings::map().has("nowindow") ? SETTING(nowindow).value<bool>() : false;

		while(!images.empty()) {
			auto p = images.front();
			images.pop();

			std::string name = p.first;
			Image *ptr = p.second;

            if(waiting.count(name)) {
                waiting[name].push(ptr);
                continue;
            }
            
            if(ptr == NULL) {
                waiting[name] = std::queue<Image*>();
                continue;
            }
            
            if(display_map.count(name)) {
                delete display_map.at(name);
            }
            
            display_map[name] = ptr;
		}
        
        for(auto &p: display_map) {
			cv::Mat mat = p.second->get();
#ifdef __linux__
			resize_image(mat, 1.5, cv::INTER_AREA);
#endif
            if(!nowindow)
                cv::imshow(p.first, mat);
            delete p.second;
        }
        
        if(!nowindow && !display_map.empty()) {
            lock.unlock();
#if !defined(__EMSCRIPTEN__)
            cv::waitKey(1);
#endif
            lock.lock();
        }
        
        for (auto &pair : waiting) {
            auto &queue = pair.second;
            Image* show = NULL;
            
            while(!queue.empty()) {
                auto first = queue.front();
                queue.pop();
                
                if(first) {
                    if(show)
                        delete show;
                    show = first;
                    
                } else {
                    break;
                }
            }
            
            if(show) {
                cv::Mat mat = show->get();
#ifdef __linux__
                resize_image(mat, 1.5, cv::INTER_AREA);
#endif
                if(!nowindow)
                    cv::imshow(pair.first, mat);
                delete show;
                if(!nowindow) {
                    lock.unlock();
#if !defined(__EMSCRIPTEN__)
                    cv::waitKey();
#endif
                    lock.lock();
                }
            }
        }
        
        for (auto &pair : waiting) {
            if(pair.second.empty()) {
                waiting.erase(pair.first);
                break;
            }
        }
	}
    
    void ThreadSafety::waitKey(std::string name) {
        LockGuard guard;
        images.push({ name, NULL });
    }
}
