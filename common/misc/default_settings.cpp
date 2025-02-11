#include "default_settings.h"

namespace cmn {
    namespace settings {
        void print_help(const sprite::Map& config, const GlobalSettings::docs_map_t& docs, const GlobalSettings::user_access_map_t*) {
            DebugHeader("DEFAULT PARAMETERS");
            size_t max_length = 0, max_value = 0;
            for(auto &k : docs)
                max_length = max(max_length, k.first.length());
            
            for (auto &k : docs) {
                auto value = config[k.first].get().valueString();
                max_value = max(max_value, value.length());
            }
            
            for (auto &k : docs) {
                auto value = config[k.first].get().valueString();
                std::string b = "";
                if(k.first.length() < max_length)
                    b = utils::repeat(" ",  max_length-k.first.length());
                
                std::string c = "";
                if(value.length() < max_value)
                    c = utils::repeat(" ", max_value-value.length());
                
                std::string doc = "";
                if(docs.find(k.first) != docs.end()) {
                    doc = docs.at(k.first);
                    /*if(doc.length() > 50) {
                     auto sub = doc.substr(0, 50);
                     Print(k,b," ",value,c," ",sub);
                     for(size_t i=50; i<doc.length(); i+=50) {
                     auto sub = doc.substr(i, min(doc.length() - i, 50u));
                     Print(sub);
                     }
                     } else
                     Print(k,b," ",value,c," ",doc);*/
                }
                
                Print(k,b.c_str()," ",value.c_str(),c.c_str()," ",doc.c_str());
            }
        }
        
        inline bool is_number(const std::string& s)
        {
            if(s == ".")
                return false;
            return !s.empty() && std::find_if(s.begin(),
                                              s.end(), [](char c) { return !std::isdigit(c) && c!='.'; }) == s.end();
        }
        
    std::string htmlify(std::string doc, bool add_links) {
        std::stringstream parsed;
        //parsed.reserve(doc.size() * 1.2);  // Reserve an estimate to avoid reallocations
        
        std::stringstream word;
        char in_string = 0;
        
        static constexpr std::array<std::string_view, 20> keywords {
            "true", "false",
            "int", "float", "double",
            "ulong", "uint", "uchar",
            "vector", "string", "vec", "size", "color",
            "bool", "array", "long", "path", "map",
            "bounds", "sizefilters"
        };
        
        auto end_word = [&](char c, char sep) {
            std::string str = word.str();
            word.str("");
            
            if(!str.empty()) {
                if(contains(keywords, str))
                    str = "<key>" + str + "</key>";
                else if(is_number(str))
                    str = "<nr>" + str + "</nr>";
            }
            
            if(sep == '`') {
                parsed << (utils::contains(str, "://") ? (add_links ? "<a href=\"" + str + "\" target=\"_blank\">" : "<a>") : "<ref>") << str << (utils::contains(str, "://") ? "</a>" : "</ref>");
            } else {
                parsed << str;
            }
            
            if(c) parsed << c;
        };

        doc = utils::find_replace(doc, "<", "&lt;");
        doc = utils::find_replace(doc, ">", "&gt;");
        
        char prev = 0;
        for(auto c : doc) {
            if(c == '\n') {
                end_word(0, 0);
                parsed << "<br/>";
            } else if(prev == '\\') {
                word << c;
                prev = 0;
                
            } else if(c == '\\') {
                word << c;
                prev = c;
                
            } else if(is_in(c, '\'', '`', '"', '$')
                      && ((in_string == 0)
                        || (in_string != 0 && in_string == c)))
            {
                bool closing = (in_string == c);
                if(closing) in_string = 0;
                else in_string = c;
                
                end_word(0, closing ? c : 0);
                
                if(c == '\'' || c == '"') {
                    if(closing)
                        parsed << c << (in_string == c ? "<str>" : "</str>");
                    else
                        parsed << (in_string == c ? "<str>" : "</str>") << c;
                } else if(c == '`') {
                    //...
                } else if(c == '$') {
                    parsed << (in_string == c ? "<h4>" : "</h4>\n");
                } else
                    parsed << c;
            } else if(in_string == 0) {
                if(c != '.' && (std::isspace(c) || !std::isalnum(c))) {
                    end_word(c, 0);
                } else {
                    word << c;
                }
            } else {
                word << c;
            }
        }
        
        if(!word.str().empty()) {
            end_word(0, 0);
        }
        
        return parsed.str();
    }

    std::string create_regex_from_array(const std::string& filter) {
        std::stringstream regex_stream;
        regex_stream << "(";
        bool first = true;

        for (const auto& param : utils::split(filter, ';')) { // Assuming `split` exists
            if (!first) regex_stream << "|";
            regex_stream << std::regex_replace(param, std::regex(R"([\^\$\.\*\+\?\(\)\[\]\{\}\|\\])"), R"(\\$&)"); // Escape special chars
            first = false;
        }

        regex_stream << ")";
        return regex_stream.str();
    }
    
    /**
     * Outputs a given set of Parameter map and documentation into the reStructuredText format (https://docutils.sourceforge.io/docs/ref/rst/directives.html). The output is a single page containing documentation for all parameters in the map, including parameter type, access type and default parameters plus a description.
     */
    std::string help_restructured_text(const std::string& title,
                                       const sprite::Map& config,
                                       const GlobalSettings::docs_map_t& docs,
                                       const GlobalSettings::user_access_map_t& fn,
                                       std::string postfix,
                                       std::string filter,
                                       std::string extra_text,
                                       AccessLevel level)
    {
        std::stringstream ss;
        
        //! prints a string with an underline of the same length made up of 'c's, usually a title
        auto print_title = [&](const std::string& str, const char c='#') {
            ss << str << std::endl;
            for (size_t i=0; i<str.size(); ++i) {
                ss << c;
            }
            ss << std::endl;
        };
        
        auto print_parameter = [&](const std::string& name, const std::string& type, const std::string& default_value, std::string description) {
            std::vector<std::string> see_also;
            bool in_parm = false;
            std::string param_name;
            std::string options = "";
            std::stringstream text;
            auto keys = config.keys();
            
            for (size_t i=0; i<description.size(); ++i) {
                if (description.at(i) == '`') {
                    in_parm = !in_parm;
                    if(!in_parm) {
                        if(param_name != name) {
                            /*if(i < description.size()-1 && description.at(i+1) == '_') {
                                description = description.substr(0, i+1) + "\\" + description.substr(i+1);
                            }*/
                            
                            if(contains(keys, param_name)) {
                                see_also.push_back(param_name);
                                text << "``" << param_name << "``";
                                
                            } else if(utils::beginsWith(param_name, "http://") || utils::beginsWith(param_name, "https://"))
                            {
                                auto sub = param_name;
                                if(sub.length() > 50) {
                                    sub = param_name.substr(0, 25) + "[...]" + param_name.substr(param_name.length() - 15 - 1);
                                    text << "`" << sub << " <" << param_name << ">`_";
                                } else
                                    text << "`<" << param_name << ">`_";
                                
                                
                            } else {
                                Print("Cannot find ",param_name," in map.");
                                text << "``" << param_name << "``";
                            }
                        }
                        
                    } else
                        param_name = "";
                    
                } else if(in_parm) {
                    param_name += description.at(i);
                    
                } else {
                    if(description.at(i) == '$') {
                        if(utils::beginsWith(description.substr(i), "$options$")) {
                            options = description.substr(i+9);
                            description = description.substr(0,i);
                            break;
                        }
                    }
                    
                    text << description.at(i);
                }
            }
            
            ss << ".. function:: " << name << "(" << type << ")" << std::endl;
            if(!postfix.empty())
                ss << "\t" << postfix << std::endl;
            ss << std::endl;
            ss << "\t" << "**default value:** " << default_value << std::endl << std::endl;
            
            if(!options.empty()) {
                ss << "\t**possible values:**" << std::endl;
                for (auto &e : utils::split(options, '\n')) {
                    ss << "\t\t- " << e << std::endl;
                }
                //ss << "\t" << options << std::endl << std::endl;
            }
            
            ss << std::endl;
            
            ss << "\t" << text.str() << std::endl << std::endl;
            
            if(!see_also.empty()) {
                ss << "\t" << ".. seealso:: ";
                size_t i=0;
                for (auto&n : see_also) {
                    if(i > 0)
                        ss << ", ";
                    ss << ":param:`" << n << "`";
                    ++i;
                }
                ss << std::endl;
            }
            
            ss << std::endl << std::endl;
        };
        
        //ss << ".. include:: names.rst" << std::endl;
        ss << ".. toctree::" << std::endl;
        ss << "   :maxdepth: 2" << std::endl << std::endl;
        
        print_title(title);
        
        if(!extra_text.empty())
            ss << extra_text;
        
        auto keys = extract_keys(docs);
        std::string regex_string = create_regex_from_array(filter);
        std::regex regex_pattern(regex_string);
        
        for (auto &key : keys) {
            if (not filter.empty()
                && std::regex_search(key, regex_pattern))
            {
                continue;
            }
            
            auto value = config[key].get().valueString();
            std::string doc = "";
            if(docs.find(key) != docs.end()) {
                doc = docs.at(key);
            }
            
            std::string access_level = "PUBLIC";
            auto it = fn.find(key);
            if(it != fn.end()) {
                if(it->second > AccessLevelType::PUBLIC)
                    access_level = it->second.name();
                if(it->second > level)
                    continue;
            }
            
            print_parameter(key, (std::string)config[key].type_name(), value, doc);
        }
        
        return ss.str();
    }
        
        std::string help_html(const sprite::Map& config, const GlobalSettings::docs_map_t& docs, const GlobalSettings::user_access_map_t& fn) {
            std::stringstream ss;
            ss << "<html><head>";
            ss << "<style>";
            ss << "map{ \
        display: table; \
            width: 100%; \
        } h4 { font-size: 16px; margin-top:3px; margin-bottom:3px; }\
        row { \
        display: table-row; \
        } \
        @font-face { \
            font-family: 'CustomFont'; \
            src: url('fonts/Quicksand-.ttf'); \
        } \
        row.header { \
            background-color: #EEE; \
        } \
        key, value, doc { \
        border: 1px solid #999999; \
        display: table-cell; \
        padding: 3px 10px; \
        } \
            row.readonly { color: gray; background-color: rgb(242, 242, 242); } \
            doc { overflow-wrap: break-word; }\
            value { overflow-wrap: break-word;max-width: 300px; }\
        row.header { \
            background-color: #EEE; \
            font-weight: bold; \
        } \
        row.footer { \
            background-color: #EEE; \
        display: table-footer-group; \
            font-weight: bold; \
        } \
            string, str { display:inline; color: red; font-style: italic; }    \
            ref { display:inline; font-weight:bold; } ref:hover { color: gray; } \
            number, nr { display:inline; color: green; } \
            keyword { display:inline; color: purple; } \
        .body { \
        display: table-row-group; \
        font-family: 'CustomFont', sans-serif; \
        font-size: 14px;    \
        }";
            
            ss <<"</style>";
            ss <<"</head><body>";
            ss << "<map><div class='body'><row class='header'><key>Name</key><value>Default value</value><doc>Description</doc></row>";
            
            for (auto &k : docs) {
                auto value = htmlify(config[k.first].get().valueString());

                std::string doc = "";
                if(docs.find(k.first) != docs.end()) {
                    doc = htmlify(docs.at(k.first), true);
                }
                
                auto key = k.first;
                auto it = fn.find(key);
                if(it != fn.end() && it->second > AccessLevelType::PUBLIC)
                    key = key + " <i>(" + it->second.name() + ")</i>";
                
                ss << "<row"<<(it != fn.end() && it->second > AccessLevelType::PUBLIC ? " class='readonly'" : "")<<"><key name='"<<k.first<<"'>" << key << "</key><value>" << value << "</value><doc>" << doc << "</doc></row>";
            }
            
            ss << "</div></map></body></html>";
            
            return ss.str();
        }
    }
}
