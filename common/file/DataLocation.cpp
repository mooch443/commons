#include "DataLocation.h"
#include <misc/detail.h>

namespace file {

static std::mutex location_mutex;
static std::map<std::string, std::function<file::Path(file::Path)>> location_funcs;

void DataLocation::register_path(std::string purpose, std::function<file::Path (file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));
    
    std::lock_guard<std::mutex> guard(location_mutex);
    if(location_funcs.find(purpose) != location_funcs.end()) {
        throw U_EXCEPTION("Purpose ",purpose," already found in map with keys ",extract_keys(location_funcs),". Cannot register twice.");
    }
    
    print("Registering purpose ", purpose);
    location_funcs.insert({purpose, fn});
}

void DataLocation::replace_path(std::string purpose, std::function<file::Path (file::Path)> fn)
{
    purpose = utils::trim(utils::lowercase(purpose));
    
    std::lock_guard<std::mutex> guard(location_mutex);
    auto it = location_funcs.find(purpose);
    if(it != location_funcs.end())
        location_funcs.erase(it);
    
    print("Replacing purpose ", purpose);
    location_funcs.insert({purpose, fn});
}

file::Path DataLocation::parse(const std::string &purpose, file::Path path) {
    std::function<file::Path(file::Path)> fn;
    {
        std::lock_guard<std::mutex> guard(location_mutex);
        auto it = location_funcs.find(utils::trim(utils::lowercase(purpose)));
        if(it == location_funcs.end()) {
            throw U_EXCEPTION("Cannot find purpose ",purpose," in map with keys ",extract_keys(location_funcs)," in order to modify path ",path,".");
        }
        
        fn = it->second;
    }
    
    return fn(path);
}

bool DataLocation::is_registered(std::string purpose) {
    purpose = utils::trim(utils::lowercase(purpose));
    
    std::lock_guard<std::mutex> guard(location_mutex);
    return location_funcs.find(purpose) != location_funcs.end();
}

}
