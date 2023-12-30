#pragma once

#include <file/Path.h>
#include <misc/SpriteMap.h>
#include <misc/GlobalSettings.h>

namespace cmn {
    class CommandLine {
        GETTER(file::Path, wd);
        
    public:
        struct Option {
            std::string name;
            std::string value;
            
            bool operator==(const std::string& other) const {
                return name == other;
            }
        };
        
    protected:
        GETTER(std::vector<Option>, options);
        GETTER(std::vector<Option>, settings);
        std::map<std::string,std::string> _settings_keys;
        
        void _init(int argc, char** argv, bool no_autoload_settings = false, const std::map<std::string, std::string>& deprecated = {});
        
    public:
        const std::map<std::string,std::string>& settings_keys() const {
            return _settings_keys;
        }
        
    public:
        static void init(int argc, char** argv, bool no_autoload_settings = false, const std::map<std::string, std::string>& deprecated = {});
        static CommandLine& instance();
        
        //! Changes the current directory to the directory of the executable
        void cd_home();
        
        //! Loads settings passed as command-line options into GlobalSettings map
        void load_settings(const SettingsMaps* additional = nullptr, sprite::Map* = nullptr, const std::vector<std::string>& exclude = {});
        
        //! Iterate custom command-line options that havent been processed already
        decltype(_options)::const_iterator begin() const { return _options.begin(); }
        decltype(_options)::const_iterator end() const { return _options.end(); }
        
        void add_setting(std::string name, std::string value);
    };
}
