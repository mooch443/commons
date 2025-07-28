#pragma once

#include <commons.pc.h>
#include <misc/GlobalSettings.h>

namespace cmn {
    namespace settings {
    ENUM_CLASS(ParameterCategoryType, CONVERTING, TRACKING)
    using ParameterCategory = ParameterCategoryType::Class;
    
        class Adding {
            Configuration& config;
            
        public:
            Adding(Configuration& config) : config(config) {}
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<is_enum<T>::value && T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
            {
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
                config.docs[name] = overall_doc;
                
                if(!config.values.has(name))
                    config.values.insert(name, default_value);
                else
                    config.values[name] = default_value;
                
                if(example_value) {
                    /// we can use this already to test whether we need to change our
                    /// documentation (example value)
                    config.values.at(name)->copy_to(config.examples);
                    config.examples[name] = *example_value;
                }
                
                config.values.at(name)->copy_to(config.defaults);
                
                if(access > AccessLevelType::PUBLIC)
                    config.set_access(name, access);
                
                return config.values[name].toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<is_enum<T>::value && !T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
            {
                config.docs[name] = doc;
                
                if(!config.values.has(name))
                    config.values.insert(name, default_value);
                else
                    config.values[name] = T(default_value);
                
                if(example_value) {
                    /// we can use this already to test whether we need to change our
                    /// documentation (example value)
                    config.values.at(name)->copy_to(config.examples);
                    config.examples[name] = *example_value;
                }
                
                config.values.at(name)->copy_to(config.defaults);
                
                if(access > AccessLevelType::PUBLIC)
                    config.set_access(name, access);
                
                return config.values[name].toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(std::string name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<!is_enum<T>::value, T>::type * = nullptr)
            {
                config.docs[name] = doc;
                
                if(!config.values.has(name))
                    config.values.insert(name, default_value);
                else
                    config.values[name] = T(default_value);
                
                if(example_value) {
                    /// we can use this already to test whether we need to change our
                    /// documentation (example value)
                    config.values.at(name)->copy_to(config.examples);
                    config.examples[name] = *example_value;
                }
                
                config.values.at(name)->copy_to(config.defaults);
                
                if(access > AccessLevelType::PUBLIC)
                    config.set_access(name, access);
                
                return config.values[name].toProperty<T>();
            }
        };
        
        void print_help(const Configuration&);
        std::string help_html(const Configuration&);
    
        std::string help_restructured_text(const std::string& title, const Configuration&, std::string postfix="", std::string filter="", std::string extra_text = "", AccessLevel level = AccessLevelType::SYSTEM);
    
        std::string htmlify(std::string str, bool add_inks = false);
    }
}
