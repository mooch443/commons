#pragma once

#include <commons.pc.h>
#include <misc/metastring.h>
#if defined(WIN32)
#include <io.h>
#endif

//#define COMMONS_FORMAT_LOG_TO_FILE

namespace cmn {

enum class FormatColor {
    BLACK = 0, DARK_BLUE = 1, DARK_GREEN = 2, DARK_CYAN = 3, DARK_RED = 4, PURPLE = 5, DARK_YELLOW = 6,
    GRAY = 7, DARK_GRAY = 8, BLUE = 9, GREEN = 10, CYAN = 11, RED = 12, PINK = 13, YELLOW = 14,
    WHITE = 15, LIGHT_BLUE, ORANGE
};

enum class FormatterType {
    UNIX,
    TAGS,
    HTML,
    NONE
};

template<uint8_t N, typename T>
    requires std::floating_point<T>
struct decimals_t {
    T value;
    std::string toStr() const {
        return Meta::toStr(value);
    }
};

template <std::size_t V, std::size_t N, class I = std::make_integer_sequence<std::size_t, N>>
struct power;

template <std::size_t V, std::size_t N, std::size_t... Is>
struct power<V, N, std::integer_sequence<std::size_t, Is...>> {
   static constexpr std::size_t value = (static_cast<std::size_t>(1) * ... * (V * static_cast<bool>(Is + 1)));
};

template<std::size_t N, typename T>
inline auto dec(T value) {
    if constexpr(N == 0) {
        return decimals_t<N, T>{ T( int64_t( value + 0.5 ) ) };
    }
    constexpr auto D = power<10u, N>::value;
    return decimals_t<N, T>{ T( int64_t( value * D + 0.5 ) ) / D };
}

template<FormatColor value, FormatterType type = FormatterType::UNIX>
    requires (type == FormatterType::UNIX)
std::string console_color(const std::string& enclose = "") {
    static std::once_flag flag;
    static bool dont_print = true;
    std::call_once(flag, []() {
#if __APPLE__
        char* xcode_colors = getenv("OS_ACTIVITY_DT_MODE");
        if (!xcode_colors || (strcmp(xcode_colors, "YES") != 0))
        {
#endif
            if (isatty(fileno(stdout)))
                dont_print = false;
#if __APPLE__
        }
#endif
    });

    if(dont_print)
        return enclose;
    
#define COLOR(PREFIX, FINAL) { prefix = PREFIX; final = FINAL; break; }
    int final = 0, prefix = 22;
    
    switch (value) {
        case FormatColor::YELLOW: COLOR(1, 33)
        case FormatColor::DARK_RED: COLOR(2, 91)
        case FormatColor::GREEN: COLOR(1, 32)
    #if defined(__APPLE__)
        case FormatColor::GRAY: COLOR(1, 30)
    #elif defined(WIN32)
        case FormatColor::GRAY: COLOR(1, 90)
    #else
        case FormatColor::GRAY: COLOR(0, 37)
    #endif
        case FormatColor::CYAN: COLOR(1,36)
        case FormatColor::DARK_CYAN: COLOR(0, 36)
        case FormatColor::PINK: COLOR(0,35)
        case FormatColor::BLUE: COLOR(1,34)
#if defined(WIN32)
        case FormatColor::BLACK: COLOR(1, 37)
        case FormatColor::WHITE: COLOR(0, 30)
        case FormatColor::DARK_GRAY: COLOR(0, 37)
#else
        case FormatColor::BLACK: COLOR(0, 30)
        case FormatColor::WHITE: COLOR(0, 37)
        case FormatColor::DARK_GRAY: COLOR(0, 30)
#endif
        case FormatColor::DARK_GREEN: COLOR(0, 32)
#if defined(WIN32)
        case FormatColor::PURPLE: COLOR(0, 95)
#else
        case FormatColor::PURPLE: COLOR(22, 35)
#endif
        case FormatColor::RED: COLOR(1, 31)
        case FormatColor::LIGHT_BLUE: COLOR(0, 94)
        case FormatColor::ORANGE: COLOR(0, 33)
            
        default:
            COLOR(0, 0)
    }
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "\033[%02d;%dm", prefix, final);
    return std::string(buffer) + (!enclose.empty() ? enclose + "\033[0m" : "");
#undef COLOR
}

template<FormatColor value, FormatterType type>
    requires (type == FormatterType::TAGS)
std::string console_color(const std::string& enclose = "") {
    std::string tag;
    switch (value) {
    case FormatColor::YELLOW: tag = "yellow"; break;
        case FormatColor::DARK_RED: tag = "darkred"; break;
        case FormatColor::GREEN: tag = "green"; break;
        case FormatColor::GRAY: tag = "gray"; break;
        case FormatColor::CYAN: tag = "cyan"; break;
        case FormatColor::DARK_CYAN: tag = "darkcyan"; break;
        case FormatColor::PINK: tag = "pink"; break;
        case FormatColor::BLUE: tag = "blue"; break;
        case FormatColor::BLACK: tag = "black"; break;
        case FormatColor::WHITE: tag = "white"; break;
        case FormatColor::DARK_GRAY: tag = "darkgray"; break;
        case FormatColor::DARK_GREEN: tag = "darkgreen"; break;
        case FormatColor::PURPLE: tag = "purple"; break;
        case FormatColor::RED: tag = "red"; break;
        case FormatColor::LIGHT_BLUE: tag = "lightblue"; break;
        case FormatColor::ORANGE: tag = "orange"; break;

        default:
            break;
    }

    if (enclose == " ")
        return " ";
    return "{" + tag + "}" + (enclose.empty() ? "" : (enclose + "{/"+tag+"}"));
}

template<FormatColor value, FormatterType type>
    requires (type == FormatterType::HTML)
std::string console_color(const std::string& enclose = "") {
    std::string tag;
    switch (value) {
    case FormatColor::YELLOW: tag = "yellow"; break;
    case FormatColor::DARK_RED: tag = "darkred"; break;
    case FormatColor::GREEN: tag = "green"; break;
    case FormatColor::GRAY: tag = "gray"; break;
    case FormatColor::CYAN: tag = "cyan"; break;
    case FormatColor::DARK_CYAN: tag = "darkcyan"; break;
    case FormatColor::PINK: tag = "pink"; break;
    case FormatColor::BLUE: tag = "blue"; break;
    case FormatColor::BLACK: tag = "black"; break;
    case FormatColor::WHITE: tag = "white"; break;
    case FormatColor::DARK_GRAY: tag = "darkgray"; break;
    case FormatColor::DARK_GREEN: tag = "darkgreen"; break;
    case FormatColor::PURPLE: tag = "purple"; break;
    case FormatColor::RED: tag = "red"; break;
    case FormatColor::LIGHT_BLUE: tag = "lightblue"; break;
    case FormatColor::ORANGE: tag = "orange"; break;

    default:
        break;
    }

    if (enclose == " ")
        return " ";
    return "<" + tag + ">" + (enclose.empty() ? "" : (utils::find_replace(enclose, { {"\n", "<br/>"},{"\t","&nbsp;&nbsp;&nbsp;&nbsp;"} }) + "</" + tag + ">"));
}

template<FormatColor value, FormatterType type>
    requires (type == FormatterType::NONE)
std::string console_color(const std::string& enclose = "") {
    return enclose;
}

template<typename T, typename U>
concept is_explicitly_convertible =
           std::is_constructible<U, T>::value
        && (!std::is_convertible<T, U>::value);

template<FormatterType colors = FormatterType::UNIX>
class ParseValue {
private:
    ParseValue() = delete;

public:
    template<typename T, typename K = std::remove_cvref_t<T>>
        requires _has_tostr_method<K>
                && (!std::convertible_to<K, std::string>)
                && (!is_explicitly_convertible<K, std::string>)
                && (!_is_number<K>)
    static std::string parse_value(const T& value) {
        auto str = value.toStr();
        return parse_value(str.c_str());
    }

    //! take all that are convertible to std::string
    //! as well as the ones that might have a toStr,
    //! but ALSO a string operator (since they can apparently
    //! be represented directly AS a string, so they ARE one).
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires (std::convertible_to<K, std::string> || is_explicitly_convertible<K, std::string>)
                && (!std::same_as<K, std::string>)
                && (!_is_dumb_pointer<T>)
    static std::string parse_value(const T& value) {
        return console_color<FormatColor::RED, colors>(Meta::toStr<std::string>((std::string)value));
    }

    static std::string pretty_text(const std::string& s) {
        std::stringstream ss;
        std::string tmp;
        enum State {
            NONE = 0,
            NUMBER = 1,
            STRING = 2,
            CLASS = 3,
            WHITESPACE = 4,
            TAG = 5,
            FUNCTION = 6,
            COMMENT = 7, MULTI_COMMENT, NAMESPACE, ESCAPED_CHAR
        };

        std::vector<State> states{ NONE };

        auto is_state = [&](State s) {
            return (states.back() == s);
        };

        auto is_keyword = [](std::string word) {
            word = utils::trim(utils::lowercase(word));
            static const std::vector<std::string> keywords{
                "string", "pv",
                "path",
                "array", "map", "set", "pair",
                "int", "float", "double", "bool", "byte", "char", "unsigned", "int64_t", "uint64_t", "int32_t", "uint32_t", "int16_t", "uint16_t", "int8_t", "uint8_t", "uint",
                "true", "false",
                "if", "def"
            };
            return contains(keywords, word);
        };

        auto alpha_numeric = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };

        auto apply_state = [&](State s) {
            if (!tmp.empty()) {
                switch (s) {
                case MULTI_COMMENT:
                    ss << console_color<FormatColor::DARK_GREEN, colors>(tmp);
                    break;
                case COMMENT:
                    ss << console_color<FormatColor::GREEN, colors>(tmp);
                    break;
                case ESCAPED_CHAR:
                        //printf("ESCAPED %s\n", tmp.c_str());
                    ss << console_color<FormatColor::DARK_RED, colors>(tmp);
                    break;
                case STRING:
                    //printf("STRING %s\n", tmp.c_str());
                    ss << console_color<FormatColor::RED, colors>() << tmp;
                    break;
                case NUMBER:
                    //printf("NUMBER %s\n", tmp.c_str());
                    ss << parse_value(Meta::fromStr<double>(tmp));
                    break;
                case CLASS:
                    //printf("CLASS %s\n", tmp.c_str());
                    ss << console_color<FormatColor::CYAN, colors>(tmp);
                    break;
                case NAMESPACE:
                    ss << console_color<FormatColor::DARK_CYAN, colors>(tmp);
                    break;
                case FUNCTION:
                    //printf("FUNCTION: %s\n", tmp.c_str());
                    if (!is_keyword(tmp))
                        ss << console_color<FormatColor::YELLOW, colors>(tmp);
                    else
                        ss << console_color<FormatColor::PURPLE, colors>(tmp);
                    break;
                case TAG:
                    //printf("TAG %s\n", tmp.c_str());
                    if (!is_keyword(tmp))
                        ss << console_color<FormatColor::CYAN, colors>(tmp);
                    else
                        ss << console_color<FormatColor::PURPLE, colors>(tmp);
                    break;
                case WHITESPACE: {
                    if (tmp == "::") {
                        //printf("TAG:: %s\n", tmp.c_str());
                        ss << console_color<FormatColor::DARK_GRAY, colors>(tmp);
                    }
                    else {
                        //printf("WHITESPACE: %s\n", tmp.c_str());
                        ss << console_color<FormatColor::BLACK, colors>(tmp);
                    }
                    break;
                }

                default:
                    if (is_keyword(tmp)) {
                        ss << console_color<FormatColor::PURPLE, colors>(tmp);
                        //printf("word: %s\n", tmp.c_str());
                    }
                    else if (s == TAG) {
                        // printf("tag: %s\n", tmp.c_str());
                        if (tmp == ">")
                            ss << console_color<FormatColor::BLACK, colors>(tmp);
                    }
                    else if (is_state(CLASS)) {
                        //printf("GClass: '%s'\n", tmp.c_str());
                        ss << console_color<FormatColor::GRAY, colors>(tmp);
                    }
                    else {
                        //printf("Regular: %s\n", tmp.c_str());
                        ss << console_color<FormatColor::BLACK, colors>(tmp);
                    }
                    break;
                }

                tmp.clear();
            }
        };

        auto pop_state = [&]() {
            if (states.empty()) {
                //printf("States array empty\n");
                return;
            }

            apply_state(states.back());
            states.pop_back();
        };

        auto push_state = [&](State s) {
            apply_state(states.back());
            states.push_back(s);
        };

        static constexpr auto whitespace = " \t\n,^()[]{}<>/&%$!?\\.:=-";

        size_t N = s.size();
        char str_char = 0;

        for (size_t i = 0; i < N; ++i) {
            if (!s[i])
                break;

            if (is_state(WHITESPACE)) {
                if (i == 0 || s[i] != s[i - 1])
                    pop_state();
            }

            if (is_state(COMMENT) || is_state(MULTI_COMMENT)) {
                if (is_state(COMMENT) && s[i] == '\n') {
                    pop_state();
                }
                else if (is_state(MULTI_COMMENT) && s[i] == '*' && i < N - 1 && s[i + 1] == '/') {
                    tmp += s[i];
                    tmp += s[i + 1];
                    ++i;
                    pop_state();
                    continue;
                }

            }
            else if (s[i] == '"' || s[i] == '\'') {
                if (!is_state(STRING)) {
                    str_char = s[i];
                    push_state(STRING);
                }
                else if (s[i] == str_char) {
                    tmp += s[i];
                    pop_state();
                    continue;
                }
            }
            else if (is_state(STRING)) {
                if (s[i] == '\\' && i < N - 1) //&& (alpha_numeric(s[i + 1]) || ))
                {
                    push_state(ESCAPED_CHAR);
                    tmp += s[i];
                    tmp += s[i + 1];
                    ++i;
                    pop_state();
                    continue;
                }
            }
            else if (!is_state(FUNCTION) && s[i] == '(') {
                if (states.back() == NAMESPACE || states.back() == CLASS) {
                    states.back() = FUNCTION;
                    pop_state();
                }
                else {
                    states.push_back(FUNCTION);
                    pop_state();
                }

                push_state(WHITESPACE); tmp += s[i]; pop_state();
                continue;
            }
            else if (!is_state(COMMENT) && s[i] == '/' && i < N - 1 && s[i + 1] == '/') {
                push_state(COMMENT);
            }
            else if (!is_state(MULTI_COMMENT) && s[i] == '/' && i < N - 1 && s[i + 1] == '*') {
                push_state(MULTI_COMMENT);
            }
            else if (s[i] == '<') {
                states.push_back(CLASS); pop_state();
                push_state(WHITESPACE); tmp += s[i]; pop_state();
                push_state(TAG);
                continue;
            }
            else if (is_state(TAG) && s[i] == '>') {
                pop_state();
                push_state(WHITESPACE); tmp += s[i]; pop_state();
                continue;
            }
            else if ((i == 0 || utils::contains(whitespace, s[i-1])) && !is_state(NUMBER) && s[i] >= '0' && s[i] <= '9') {
                push_state(NUMBER);
            }
            else if (is_state(NUMBER)) {
                if (s[i] == '.' || (s[i] >= '0' && s[i] <= '9') || s[i] == 'f') {
                }
                else
                    pop_state();
            }
            else if (utils::contains(whitespace, s[i])) {
                if (!is_state(WHITESPACE)) {
                    while (is_state(CLASS) || is_state(NAMESPACE)) {
                        pop_state();
                    }

                    if (s[i] == ':' && i < N - 1 && s[i + 1] == s[i]) {
                        states.push_back(NAMESPACE);
                        pop_state();
                        push_state(WHITESPACE); tmp += s[i]; tmp += s[i + 1]; pop_state();
                        push_state(CLASS);
                        ++i;
                        continue;
                    }

                    push_state(WHITESPACE);
                }
            }

            tmp += s[i];
        }

        if (!tmp.empty())
            pop_state();

        return ss.str();
    }

    template<typename T, size_t N, typename K = cmn::remove_cvref_t<T>>
        requires std::convertible_to<T[N], std::string>
    static std::string parse_value(T(&s)[N]) {
        return parse_value((const char*)s);
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires _is_dumb_pointer<T> && std::same_as<T, const char*>
    static std::string parse_value(T s) {
        return pretty_text(s);
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires _is_smart_pointer<K>
    static std::string parse_value(const T& s) {
        return "ptr<"+ console_color<FormatColor::CYAN, colors>( Meta::name<typename K::element_type>() ) + ">"
        + (s ? parse_value(*s) : console_color<FormatColor::PURPLE, colors>("null"));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires (!std::same_as<bool, K>)
            && _is_number<K>
    static std::string parse_value(T && value) {
        return console_color<FormatColor::GREEN, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::same_as<bool, K>
    static std::string parse_value(T value) {
        return console_color<FormatColor::PURPLE, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::is_enum<K>::value
    static std::string parse_value(T value) {
        return parse_value((int)value);
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires is_instantiation<cv::Size_, T>::value
    static std::string parse_value(T value) {
        return console_color<FormatColor::BLACK, colors>("[")
            + parse_value(value.width)
            + console_color<FormatColor::BLACK, colors>(","),
            parse_value(value.height)
            + console_color<FormatColor::BLACK, colors>("]");
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::same_as<K, std::string>
    static std::string parse_value(const T& value) {
        return console_color<FormatColor::RED, colors>(Meta::toStr(value));
    }
    
            
    template<class TupType, size_t... I>
    static std::string tuple_str(const TupType& _tup, std::index_sequence<I...>)
    {
                
        std::stringstream str;
        str << console_color<FormatColor::BLACK, colors>("[");
        (..., (str << (I == 0? "" : console_color<FormatColor::BLACK, colors>(",")) << parse_value(std::get<I>(_tup))));
        str << console_color<FormatColor::BLACK, colors>("]");
        return str.str();
    }
            
    template<class... Q>
    static std::string tuple_str (const std::tuple<Q...>& _tup)
    {
        return tuple_str(_tup, std::make_index_sequence<sizeof...(Q)>());
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires is_instantiation<std::tuple, K>::value
    static std::string parse_value(const T& value) {
        return tuple_str(value);
    }

    template<template <typename, typename> class T, typename A, typename B>
        requires is_pair<T<A, B>>::value
    static std::string parse_value(const T<A, B>& m) {
        return console_color<FormatColor::DARK_GRAY, colors>("[")
            + parse_value(m.first)
            + console_color<FormatColor::GRAY, colors>(",")
            + parse_value(m.second)
            + console_color<FormatColor::DARK_GRAY, colors>("]");
    }

#ifdef __llvm__
    template<template <typename ...> class T, typename... Args>
        requires is_instantiation<std::__bit_const_reference, T<Args...>>::value
    static std::string parse_value(const T<Args...>&m) {
        return parse_value((bool)m);
    }
#endif
    
    template<template <typename...> class T, typename... Args, typename K = cmn::remove_cvref_t<T<Args...>>>
        requires is_set<K>::value || is_container<K>::value
    static std::string parse_value(const T<Args...>&m) {
        std::string str = console_color<FormatColor::BLACK, colors>("[");
        auto it = m.begin(), end = m.end();
        for (;;) {
            str += parse_value(*it);

            if (++it != end)
                str += console_color<FormatColor::BLACK, colors>(",");
            else
                break;
        }
        return str + console_color<FormatColor::BLACK, colors>("]");
    }

    template<bool A, size_t S, typename... Args>
    static std::string parse_value(const robin_hood::detail::Table<A, S, Args...>& m) {
        std::string str = console_color<FormatColor::BLACK, colors>("{");
        for (auto& [k, v] : m) {
            str += console_color<FormatColor::DARK_GRAY, colors>("[")
                + parse_value(k)
                + console_color<FormatColor::GRAY, colors>(":")
                + parse_value(v)
                + console_color<FormatColor::DARK_GRAY, colors>("]");
        }
        return str + console_color<FormatColor::BLACK, colors>("}");
    }
    
    template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
        requires is_map<K>::value
    static std::string parse_value(const T<Args...>& m) {
        std::string str = console_color<FormatColor::BLACK, colors>("{");
        for (auto& [k, v] : m) {
            str += console_color<FormatColor::DARK_GRAY, colors>("[")
                + parse_value(k)
                + console_color<FormatColor::GRAY, colors>(":")
                + parse_value(v)
                + console_color<FormatColor::DARK_GRAY, colors>("]");
        }
        return str + console_color<FormatColor::BLACK, colors>("}");
    }

    template<typename T>
        requires _is_dumb_pointer<T> && (!std::same_as<const char*, T>)
    static std::string parse_value(T ptr) {
        std::string str;
        if constexpr (_has_class_name<std::remove_pointer_t<T>>)
            str += console_color<FormatColor::BLACK, colors>("(")
            + console_color<FormatColor::CYAN, colors>((std::remove_pointer_t<T>::class_name()))
            + console_color<FormatColor::BLACK, colors>("*)");
        if (!ptr)
            return str + console_color<FormatColor::PURPLE, colors>("null");
        if constexpr (_has_tostr_method<std::remove_pointer_t<T>>)
            return str + parse_value(*ptr);

        std::stringstream stream;
        stream << "0x" << std::hex << (uint64_t)ptr;
        return str + console_color<FormatColor::LIGHT_BLUE, colors>(stream.str());
    }
};

template<FormatterType type, typename... Args>
std::string format(const Args& ... args) {
    return (... + (ParseValue<type>::parse_value(args)));
}

#ifdef COMMONS_FORMAT_LOG_TO_FILE
void log_to_file(std::string);
#endif

inline std::string current_time_string() {
    using namespace std::chrono;
    return date::format("%H:%M:%S", date::floor<seconds>(system_clock::now()));
}

template<typename... Args>
void print(const Args & ... args) {
    auto str =
        "["
        + console_color<FormatColor::CYAN, FormatterType::UNIX>( current_time_string() )
        + "] "
        + console_color<FormatColor::BLACK, FormatterType::UNIX>( format<FormatterType::UNIX>(args...) );
    
    printf("%s\n", str.c_str());

#ifdef COMMONS_FORMAT_LOG_TO_FILE
    str = format<FormatterType::HTML>(args...) + "<br/>\n";
    log_to_file(str);
#endif
}

}
