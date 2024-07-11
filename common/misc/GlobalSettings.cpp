/**
 * Project Untitled
 */

#include "GlobalSettings.h"

namespace cmn {

void GlobalSettings::set_instance(GlobalSettings* ptr) {
    instance(ptr);
}

std::mutex& GlobalSettings::mutex() {
    static std::mutex _mutex;
    return _mutex;
}

/**
 * GlobalSettings implementation
 */

GlobalSettings::GlobalSettings() {
    _map.set_print_by_default(false);
}

/**
 * Destructor of @class GlobalSettings.
 */
GlobalSettings::~GlobalSettings() {
}

bool GlobalSettings::is_runtime_quiet(const sprite::Map* ptr) {
    if(not ptr)
        ptr = &GlobalSettings::map();
    return ptr->has("quiet") && ptr->at("quiet").value<bool>();
}

/**
 * Returns a reference to the settings map.
 * @return sprite::Map&
 */
sprite::Map& GlobalSettings::map() {
    if(!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_map;
}

sprite::Map& GlobalSettings::current_defaults() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_current_defaults;
}

void GlobalSettings::set_current_defaults(const sprite::Map& map) {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    instance()->_current_defaults.set_print_by_default(false);
    instance()->_current_defaults = map;
}

sprite::Map& GlobalSettings::current_defaults_with_config() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_current_defaults_with_config;
}

void GlobalSettings::set_current_defaults_with_config(const sprite::Map& map) {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    instance()->_current_defaults_with_config.set_print_by_default(false);
    instance()->_current_defaults_with_config = map;
}

const sprite::Map& GlobalSettings::defaults() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_defaults;
}

sprite::Map& GlobalSettings::set_defaults() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_defaults;
}

GlobalSettings::docs_map_t& GlobalSettings::docs() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_doc;
}

GlobalSettings::user_access_map_t& GlobalSettings::access_levels() {
    if (!instance())
        throw U_EXCEPTION("No GlobalSettings instance.");
    return instance()->_access_levels;
}

/**
 * @param name
 * @return sprite::Reference
 */
sprite::Reference GlobalSettings::_get(const char* name) {
    std::lock_guard<std::mutex> lock(mutex());
    return map()[name];
}

const std::string& GlobalSettings::doc(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex());
    return docs()[name];
}

/**
 * @param name
 * @return true if the property exists
 */
bool GlobalSettings::has(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex());
    return map().has(name);
}

bool GlobalSettings::has_doc(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex());
    return docs().find(name) != docs().end();
}

AccessLevel GlobalSettings::access_level(const std::string_view &name) {
    std::lock_guard<std::mutex> lock(mutex());
    auto it = access_levels().find(name);
    if(it == access_levels().end())
        return AccessLevelType::PUBLIC;
    return it->second;
}

void GlobalSettings::set_access_level(const std::string& name, AccessLevel w) {
    std::lock_guard<std::mutex> lock(mutex());
    access_levels()[name] = w;
}

bool GlobalSettings::has_access(const std::string &name, AccessLevel level) {
    return level >= access_level(name);
}

/**
 * Loads parameters from a file.
 * @param filename Name of the file
 */
std::map<std::string, std::string> GlobalSettings::load_from_file(const std::map<std::string, std::string>& deprecations, const std::string &filename, AccessLevel access, const std::vector<std::string>& exclude, sprite::Map* target, const sprite::Map* additional) {
    return load_from_string(sprite::MapSource{filename}, deprecations, target ? *target : GlobalSettings::map(), utils::read_file(filename), access, false, exclude, additional);
}

/**
 * Loads parameters from a string.
 * @param str the string
 */
std::map<std::string, std::string> GlobalSettings::load_from_string(sprite::MapSource source, const std::map<std::string, std::string>& deprecations, sprite::Map& map, const std::string &file, AccessLevel access, bool correct_deprecations, const std::vector<std::string>& exclude, const sprite::Map* additional) {
    /*struct G {
        std::string s;
        G(const std::string& name) : s(name) {
            DebugHeader("// LOADING FROM ", s);
        }
        ~G() {
            DebugHeader("// LOADED ", s);
        }
    } g(source.name);*/

    std::stringstream line;
    std::map<std::string, std::string> rejected;
    
    for (size_t i=0; i<=file.length(); i++) {
        auto c = i >= file.length() ? '\n' : file.at(i);
        if (c == '\n' ) {
            auto str = utils::trim(line.str());
            
            try {
                if (!str.empty() && utils::contains(str, "=") && !utils::beginsWith(str, '#')) {
                    auto parts = utils::split(str, '=');
                    if (parts.size() == 2) {
                        auto var = (std::string)utils::trim(parts.at(0));
                        auto val = (std::string)utils::trim(parts.at(1));
                        
                        if(access_level(var) <= access
                           && not contains(exclude, var))
                        {
                            std::string lower(utils::lowercase(var));
                            auto it = deprecations.find(lower);
                            if(it != deprecations.end()) {
                                if(correct_deprecations) {
                                    auto& resolution = deprecations.at(lower);
                                    if(not map.has(resolution)
                                       && additional
                                       && additional->has(resolution))
                                    {
                                        additional->at(resolution).get().copy_to(&map);
                                    }
                                    
                                    auto& obj = map[resolution].get();
                                    obj.set_value_from_string(val);
                                }
                                
                            } else if(map.has(var)) {
                                auto& obj = map[var].get();
                                obj.set_value_from_string(val);
                            } else if(additional && additional->has(var)) {
                                additional->at(var).get().copy_to(&map);
                                map[var].get().set_value_from_string(val);
                            } else {
                                sprite::parse_values(source, map,"{"+var+":"+val+"}");
                            }
                            
                            rejected.insert({var, val});
                        }
                    }
                }
            } catch(const UtilsException& e) {
                if(GlobalSettings::is_runtime_quiet())
                {
                    if(str.length() > 150) {
                        auto fr = str.substr(0, 50);
                        auto en = str.substr(str.length() - 50 - 1);
                        
                        str = fr + " [...] " + en;
                    }
                    Print("Line ", str," cannot be loaded. (",std::string(e.what()),")");
                }
            }
            
            line.str("");
            
        } else if(c != '\r') {
            line << c;
        }
    }
    
    return rejected;
}

}
