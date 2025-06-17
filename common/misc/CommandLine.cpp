#include "CommandLine.h"
#include <misc/GlobalSettings.h>
#if __APPLE__
#  include <mach-o/dyld.h>
#endif
#if WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

namespace cmn {
using namespace cmn::file;

CommandLine& CommandLine::instance() {
    static CommandLine cmd;
    return cmd;
}

void CommandLine::init(int argc, char **argv, bool no_autoload_settings, const std::map<std::string, std::string>& deprecated) {
    instance()._init(argc, argv, no_autoload_settings, deprecated);
}

    void CommandLine::_init(int argc, char** argv, bool no_autoload_settings, const std::map<std::string, std::string>& deprecated)
    {
        _wd = Path(argv[0]).remove_filename();
        
        const size_t bufSize = PATH_MAX + 1;
        char dirNameBuffer[bufSize];
        
#if __APPLE__
        uint32_t ret = bufSize;
        
        if (_NSGetExecutablePath(dirNameBuffer, &ret) != 0) {
            // Buffer size is too small.
            FormatError("NSGetExecutablePath failed.");
        } else {
            ret = (uint32_t)strlen(dirNameBuffer);
#elif WIN32
		HMODULE hModule = GetModuleHandleW(NULL);
		WCHAR path[MAX_PATH];
		auto ret = GetModuleFileNameW(hModule, path, MAX_PATH);

		if(ret == ERROR_SUCCESS) {
			std::wstring w(path);
			auto str = std::string(w.begin(), w.end());

#else // not ARCH_darwin_14_i86
        const char *linkName = "/proc/self/exe";
        const int ret = int(readlink(linkName, dirNameBuffer, bufSize - 1));
        
        if (ret == -1) {
            perror("readlink");
        } else {
                
#endif

#ifndef WIN32
            dirNameBuffer[ret] = 0;
			auto str = std::string(dirNameBuffer);
#endif
            std::string::size_type last = str.size() - 1;
            std::string::size_type idx  = str.rfind(file::Path::os_sep(), last);
            str.erase(idx + 1);
            
            _wd = str;
        }
            
#if __APPLE__
        auto wd = _wd / ".."/ "Resources";
        if(wd.exists())
            _wd = wd.absolute();
#endif
        
        SETTING(wd) = _wd;
        
        /**
         * Process command-line options.
         */
        const char *argptr = NULL;
        auto keys = GlobalSettings::map().keys();
            
        auto check_option = [&keys, &deprecated, this](const char* argptr, const char* val)
        {
            if(!argptr)
                return;
            
            std::string sval(val ? val : "");
            size_t offset = 0;
            for(size_t i=0; i+1<sval.length(); ++i) {
                if(sval[i] == '\'' || sval[i] == '"') {
                    if(sval[sval.length()-1-i] == sval[i]) {
                        offset = i+1;
                    } else break;
                } else break;
            }
            
            if(offset)
                sval = sval.substr(offset, sval.length()-offset*2);
            
            std::string key = argptr;
            if(contains(keys, key)) {
                _settings.push_back({key, sval});
                
            } else if(deprecated.find(key) != deprecated.end()) {
                FormatWarning("Found deprecated key ", key," = '", val ? val : "","' in command-line (replaced by ", deprecated.at(key),").");
                _settings.push_back({deprecated.at(key), sval});
                
            } else {
                _options.push_back({key, sval});
            }
        };
        
        for (int i=1; i<argc; i++) {
            if (argv[i][0] == '-' && (/*argv[i][1] == 0 || */argv[i][1] > '9' || argv[i][1] < '0')) {
                check_option(argptr, nullptr);
                argptr = *argv[i] ? (argv[i]+1) : argv[i];
                
            } else if(argptr) {
                check_option(argptr, argv[i]);
                argptr = NULL;
            } else
                argptr = NULL;
        }
        
        check_option(argptr, nullptr);
        
        if(!no_autoload_settings)
            load_settings();
    }
    
    void CommandLine::load_settings(const sprite::Map* additional, sprite::Map* map, const std::vector<std::string>& exclude) {
        if(not map)
            map = &GlobalSettings::map();
        if(map != &GlobalSettings::map()
           && not additional)
        {
            additional = &GlobalSettings::map();
        }

        // Helper to check if a string is properly quoted with matching single or double quotes
        static constexpr auto is_quoted = [](const std::string& v) -> bool {
            return !v.empty() &&
                ((v.front() == '\'' && v.back() == '\'') ||
                    (v.front() == '"' && v.back() == '"'));
            };
        
        auto keys = GlobalSettings::map().keys();
        for(auto it = _options.begin(); it != _options.end();) {
            auto &option = *it;
            if(contains(keys, utils::lowercase(option.name))) {
                _settings.push_back(option);
                it = _options.erase(it);
            } else
                ++it;
        }
            
        for(auto &s : _settings) {
            if(contains(exclude, s.name))
                continue;
            
            std::string value = s.value ? *s.value : "";
            sprite::PropertyType* prop{nullptr};
            if(map->has(s.name))
                prop = &(*map)[s.name].get();
            else if (additional && additional->has(s.name)) {
				additional->at(s.name).get().copy_to(*map);
                prop = &(*map)[s.name].get();
            }

            if(value.empty()) {
                if(prop
                   && prop->is_type<bool>())
                {
                    value = "true"; // by default set option to true if its bool and no value was given
                }
            }
            
            // If the property is a string-like type and the value is not quoted, quote it for parsing
            if(prop)
				prop->set_value_from_string(value);
            else {
				/// in theory this case should barely ever be of use, but it is here for completeness
                if (is_quoted(value)) {
                    sprite::parse_values(sprite::MapSource::CMD, *map, "{'" + s.name + "':" + value + "}", additional, exclude, {});
                }
                else {
                    // If the value is not quoted, we assume it is a string and quote it
                    sprite::parse_values(sprite::MapSource::CMD, *map, "{'" + s.name + "':" + Meta::toStr(value) + "}", additional, exclude, {});
                }
            }
            _settings_keys[s.name] = value;
        }
    }
            
    void CommandLine::add_setting(std::string name, std::string value) {
        _settings.push_back(Option{.name = name, .value = value});
    }
            
    void CommandLine::reset_settings(const std::vector<std::string_view> &exclude) {
        _settings.erase(std::remove_if(_settings.begin(), _settings.end(), [&](Option& option) -> bool{
            if(not contains(exclude, option.name))
                return true;
            return false;
        }), _settings.end());
        
        for(auto it = _settings_keys.begin(); it != _settings_keys.end(); ) {
            if(not contains(exclude, it->first)) {
                it = _settings_keys.erase(it);
            } else
                ++it;
        }
    }
    
    void CommandLine::cd_home() {
        file::cd(_wd);
    }
}
