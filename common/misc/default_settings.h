#pragma once

#include <commons.pc.h>
#include <misc/GlobalSettings.h>

namespace cmn {
    namespace settings {
    ENUM_CLASS(ParameterCategoryType, CONVERTING, TRACKING)
    using ParameterCategory = ParameterCategoryType::Class;
    
        class Adding {
            sprite::Map &config;
            GlobalSettings::docs_map_t &docs;
            std::function<void(const std::string& name, AccessLevel w)> fn;
            
        public:
            Adding(sprite::Map& config, GlobalSettings::docs_map_t& docs, decltype(fn) function) : config(config), docs(docs), fn(function) {}
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, typename std::enable_if<is_enum<T>::value && T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
            {
                if(access > AccessLevelType::PUBLIC && fn)
                    fn(name, access);
                
                std::string overall_doc = doc;
                auto fdocs = T::Data::template docs<T>();
                auto &fields = T::fields();
                
                if(!fields.empty())
                    overall_doc += "\n$options$";
                
                for(size_t i=0; i<fields.size(); ++i) {
                    auto n = std::string(fields.at(i));
                    auto d = std::string(fdocs.at(i));
                    
                    if(i > 0) overall_doc += "\n";
                    overall_doc += "`"+std::string(n)+"`: "+(d.empty() ? "<no description>" : d);
                }
                docs[name] = overall_doc;
                
                if(!config.has(name))
                    config.insert(name, default_value);
                else
                    config[name] = default_value;
                
                return config[name].toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, typename std::enable_if<is_enum<T>::value && !T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
            {
                if(access > AccessLevelType::PUBLIC && fn)
                    fn(name, access);
                docs[name] = doc;
                
                if(!config.has(name))
                    config.insert(name, default_value);
                else
                    config[name] = T(default_value);
                
                return config[name].toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, typename std::enable_if<!is_enum<T>::value, T>::type * = nullptr)
            {
                if(access > AccessLevelType::PUBLIC && fn)
                    fn(name, access);
                docs[name] = doc;
                
                if(!config.has(name))
                    config.insert(name, default_value);
                else
                    config[name] = T(default_value);
                
                return config[name].toProperty<T>();
            }
        };
        
        void print_help(const sprite::Map& config, const GlobalSettings::docs_map_t& docs, const GlobalSettings::user_access_map_t* fn);
        std::string help_html(const sprite::Map& config, const GlobalSettings::docs_map_t& docs, const GlobalSettings::user_access_map_t& fn);
    std::string help_restructured_text(const std::string& title, const sprite::Map& config, const GlobalSettings::docs_map_t& docs, const GlobalSettings::user_access_map_t& fn, std::string postfix="", std::string filter="", std::string extra_text = "", AccessLevel level = AccessLevelType::SYSTEM);
        std::string htmlify(std::string str, bool add_inks = false);
    }
}
