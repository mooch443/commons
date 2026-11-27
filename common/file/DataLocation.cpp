#include "DataLocation.h"
#include <misc/CommandLine.h>
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

DataLocation& DataLocation::create() {
    static DataLocation _owned_instance{};
    set_instance(&_owned_instance);
    return _owned_instance;
}

DataLocation* DataLocation::instance_if_set() noexcept {
    auto guard = LOGGED_LOCK(instance_mutex());
    return _instance;
}

DataLocation* DataLocation::instance() {
    DataLocation* result;
    {
        auto guard = LOGGED_LOCK(instance_mutex());
        result = _instance;
    } // release lock before any throw so U_EXCEPTION cannot re-enter
    if (!result)
        throw U_EXCEPTION("DataLocation::create() must be called before accessing the instance.");
    return result;
}

void DataLocation::unregister_locations() {
    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    ptr->location_funcs.clear();
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
    if(not READ_SETTING_WITH_DEFAULT(quiet, false))
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
    //Print("Replacing purpose ", purpose);
#endif
    ptr->location_funcs.insert({purpose, fn});
}

// Copy the resolver under the registry lock so callers can invoke it after releasing the lock.
DataLocation::Resolver DataLocation::lookup(const std::string& purpose) {
    auto ptr = instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    const auto it = ptr->location_funcs.find(utils::trim(utils::lowercase(purpose)));
    if(it == ptr->location_funcs.end()) {
        FormatExcept("Cannot find purpose ", purpose, " in map with keys ", extract_keys(ptr->location_funcs), ".");
        return {};
    }
    if(not it->second)
        throw InvalidArgumentException("Cannot find path resolver for purpose ", purpose, ".");
    return it->second;
}

file::Path DataLocation::parse(const std::string& purpose, file::Path path, const sprite::Map* settings) {
    const auto resolve = lookup(purpose);
    if(not resolve)
        return path;
    if(settings)
        return resolve(*settings, path);
    return GlobalSettings::read([&](const Configuration& config) {
        return resolve(config.values, path);
    });
}

// Resolve each source match against the launch directory using the same settings map.
PathArray DataLocation::parse_files(const std::string& purpose, const PathArray& source, const sprite::Map* settings) {
    auto base = CommandLine::instance().launch_dir();
    if(base.empty())
        base = file::cwd();
    const auto expanded = source.resolved_at(base);
    if(expanded.empty())
        return expanded;

    const auto resolve = lookup(purpose);
    const auto apply = [&](const sprite::Map& map) {
        std::vector<Path> paths;
        paths.reserve(expanded.size());
        for(const auto& path : expanded) {
            auto absolute = path.is_absolute() ? path : base / path;
            paths.push_back(resolve ? resolve(map, absolute) : absolute);
        }
        return PathArray::from_files(std::move(paths));
    };
    if(settings)
        return apply(*settings);
    return GlobalSettings::read([&](const Configuration& config) {
        return apply(config.values);
    });
}

bool DataLocation::is_registered(std::string purpose) {
    purpose = utils::trim(utils::lowercase(purpose));

    auto ptr = DataLocation::instance();
    std::lock_guard<std::mutex> guard(ptr->location_mutex);
    return ptr->location_funcs.contains(purpose);
}

}
