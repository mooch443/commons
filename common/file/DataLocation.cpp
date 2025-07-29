#include "DataLocation.h"
#include <misc/GlobalSettings.h>

namespace cmn::file {

static auto& instance_mutex() {
    static auto m = new LOGGED_MUTEX("DataLocation::instance_mutex");
    return *m;
}
static DataLocation* _instance = nullptr;

void DataLocation::set_instance(DataLocation* ptr) {
    auto guard = LOGGED_LOCK(instance_mutex());
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
    auto guard = LOGGED_LOCK(instance_mutex());
    if (!_instance) {
		_instance = new DataLocation();
	}
	return _instance;
}

void DataLocation::register_path(std::string purpose, std::function<file::Path (const sprite::Map&, file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));
    
    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    if(ptr->location_funcs.find(purpose) != ptr->location_funcs.end()) {
        throw U_EXCEPTION("Purpose ",purpose," already found in map with keys ",extract_keys(ptr->location_funcs),". Cannot register twice.");
    }
    
#ifndef NDEBUG
    Print("Registering purpose ", purpose);
#endif
    ptr->location_funcs.insert({purpose, fn});
}

void DataLocation::replace_path(std::string purpose, std::function<file::Path (const sprite::Map&, file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));

    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    auto it = ptr->location_funcs.find(purpose);
    if(it != ptr->location_funcs.end())
        ptr->location_funcs.erase(it);
    
#ifndef NDEBUG
    Print("Replacing purpose ", purpose);
#endif
    ptr->location_funcs.insert({purpose, fn});
}

file::Path DataLocation::parse(const std::string &purpose, file::Path path, const sprite::Map* settings) {
    std::function<file::Path(const sprite::Map&, file::Path)> fn;
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
    
    if(settings) {
        return fn(*settings, path);
    }
    
    return GlobalSettings::read([&fn, path](const Configuration& config){
        return fn(config.values, path);
    });
}

bool DataLocation::is_registered(std::string purpose) {
    purpose = utils::trim(utils::lowercase(purpose));

    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    return ptr->location_funcs.find(purpose) != ptr->location_funcs.end();
}

}
