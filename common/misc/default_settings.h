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
            sprite::Property<T>& add(StringLike auto&& name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<is_enum<T>::value && T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
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
                
                return config.values[name].template toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(StringLike auto&& name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<is_enum<T>::value && !T::Data::template enum_has_docs<T>::value, T>::type * = nullptr)
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
                
                return config.values[name].template toProperty<T>();
            }
            
            template<ParameterCategory category, typename T>
            sprite::Property<T>& add(StringLike auto&& name, T default_value, std::string doc, AccessLevel access = AccessLevelType::PUBLIC, std::optional<T> example_value = std::nullopt, typename std::enable_if<!is_enum<T>::value, T>::type * = nullptr)
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
                
                return config.values[name].template toProperty<T>();
            }
        };
        
        void print_help(const Configuration&);
        std::string help_html(const Configuration&);
    
        std::string help_restructured_text(const std::string& title, const Configuration&, std::string postfix="", std::string filter="", std::string extra_text = "", AccessLevel level = AccessLevelType::SYSTEM);
        
        std::string htmlify(StringLike auto&& str, bool add_links = false) {
            const std::string_view sv = utils::string_like_view(str);

            static constexpr std::array<std::string_view, 20> keywords {
                "true", "false",
                "int", "float", "double",
                "ulong", "uint", "uchar",
                "vector", "string", "vec", "size", "color",
                "bool", "array", "long", "path", "map",
                "bounds", "sizefilters"
            };

            auto is_number = [](std::string_view s) noexcept -> bool {
                if(s == ".") return false;
                return !s.empty() && std::find_if(s.begin(), s.end(),
                    [](unsigned char c){ return !std::isdigit(c) && c != '.'; }) == s.end();
            };

            std::string parsed;
            parsed.reserve(sv.size() * 5 / 4);
            std::string word;
            word.reserve(32);
            char in_string = 0;
            char prev = 0;

            // Append a char to a buffer, escaping < and > as HTML entities inline
            auto emit = [](std::string& out, char c) {
                if(c == '<')      out += "&lt;";
                else if(c == '>') out += "&gt;";
                else              out += c;
            };

            auto end_word = [&](char c, char sep) {
                if(!word.empty()) {
                    if(contains(keywords, word))
                        word = "<key>" + word + "</key>";
                    else if(is_number(word))
                        word = "<nr>" + word + "</nr>";
                }

                if(sep == '`') {
                    if(utils::contains(word, "://")) {
                        parsed += add_links ? "<a href=\"" + word + "\" target=\"_blank\">" : "<a>";
                        parsed += word;
                        parsed += "</a>";
                    } else {
                        parsed += "<ref>";
                        parsed += word;
                        parsed += "</ref>";
                    }
                } else {
                    parsed += word;
                }
                word.clear();

                if(c) emit(parsed, c);
            };

            for(char c : sv) {
                if(c == '\n') {
                    end_word(0, 0);
                    parsed += "<br/>";
                } else if(prev == '\\') {
                    emit(word, c);
                    prev = 0;
                } else if(c == '\\') {
                    emit(word, c);
                    prev = c;
                } else if(is_in(c, '\'', '`', '"', '$')
                          && ((in_string == 0) || (in_string == c)))
                {
                    bool closing = (in_string == c);
                    if(closing) in_string = 0;
                    else        in_string = c;

                    end_word(0, closing ? c : 0);

                    if(c == '\'' || c == '"') {
                        if(closing) {
                            parsed += c;
                            parsed += "</str>";
                        } else {
                            parsed += "<str>";
                            parsed += c;
                        }
                    } else if(c == '$') {
                        parsed += (in_string == c ? "<h4>" : "</h4>\n");
                    }
                    // '`' is consumed silently; end_word handles wrapping via sep
                } else if(in_string == 0) {
                    if(c != '.' && (std::isspace((unsigned char)c) || !std::isalnum((unsigned char)c))) {
                        end_word(c, 0);
                    } else {
                        emit(word, c);
                    }
                } else {
                    emit(word, c);
                }
            }

            if(!word.empty())
                end_word(0, 0);

            return parsed;
        }
    }
}
