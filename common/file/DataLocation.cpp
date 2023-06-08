#include "DataLocation.h"
#include <misc/detail.h>

namespace file {
static std::mutex instance_mutex;
static DataLocation* _instance = nullptr;

void DataLocation::set_instance(DataLocation* ptr) {
    std::unique_lock guard(instance_mutex);
    // we are not deleting right now, because we don't know if the instance is still in use
    // probably should reconsider this mechanism
    /*if (_instance) {
		delete _instance;
	}*/
    if (_instance != ptr) {
        _instance = ptr;
    }
}

DataLocation* DataLocation::instance() {
    std::unique_lock guard(instance_mutex);
    if (!_instance) {
		_instance = new DataLocation();
	}
	return _instance;
}

void DataLocation::register_path(std::string purpose, std::function<file::Path (file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));
    
    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    if(ptr->location_funcs.find(purpose) != ptr->location_funcs.end()) {
        throw U_EXCEPTION("Purpose ",purpose," already found in map with keys ",extract_keys(ptr->location_funcs),". Cannot register twice.");
    }
    
    print("Registering purpose ", purpose);
    ptr->location_funcs.insert({purpose, fn});
}

void DataLocation::replace_path(std::string purpose, std::function<file::Path (file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));

    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    auto it = ptr->location_funcs.find(purpose);
    if(it != ptr->location_funcs.end())
        ptr->location_funcs.erase(it);
    
    print("Replacing purpose ", purpose);
    ptr->location_funcs.insert({purpose, fn});
}

file::Path DataLocation::parse(const std::string &purpose, file::Path path) {
    std::function<file::Path(file::Path)> fn;
    {
        auto ptr = DataLocation::instance();
        std::lock_guard<std::mutex> guard(ptr->location_mutex);
        auto it = ptr->location_funcs.find(utils::trim(utils::lowercase(purpose)));
        if(it == ptr->location_funcs.end()) {
            FormatExcept("Cannot find purpose ",purpose," in map with keys ",extract_keys(ptr->location_funcs)," in order to modify path ",path,".");
            return path;
        }
        
        fn = it->second;
    }
    
    return fn(path);
}

bool DataLocation::is_registered(std::string purpose) {
    purpose = utils::trim(utils::lowercase(purpose));

    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    return ptr->location_funcs.find(purpose) != ptr->location_funcs.end();
}

}
