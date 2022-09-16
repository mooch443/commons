#pragma once

#include <commons.pc.h>
#include <misc/metastring.h>
#if defined(WIN32)
#include <io.h>
#endif

#define COMMONS_FORMAT_LOG_TO_FILE true

namespace cmn {

ENUM_CLASS(FormatColorNames,
    BLACK, 
    DARK_BLUE, 
    DARK_GREEN, 
    DARK_CYAN, 
    DARK_RED,
    PURPLE,
    DARK_YELLOW,
    GRAY, 
    DARK_GRAY, 
    BLUE, 
    GREEN, 
    CYAN, 
    RED, 
    PINK, 
    YELLOW,
    WHITE, 
    DARK_PINK,
    LIGHT_CYAN,
    LIGHT_GRAY,
    ORANGE,
    INVALID);

//static_assert(std::is_trivial_v<FormatColorNames::Class>, "Trivial enums please.");

using FormatColor_t = FormatColorNames::Class;
namespace FormatColor = FormatColorNames;

enum class FormatterType {
    UNIX,
    TAGS,
    HTML,
    NONE
};

namespace PrefixLiterals {
    enum Prefix {
        WARNING,
        ERROR_PREFIX,
        EXCEPT,
        INFO
    };

    extern const std::array<const char*, 4> names;
};

template<uint8_t N, typename T>
    requires std::floating_point<T>
struct decimals_t {
    T value;
    std::string toStr() const {
        std::string str = Meta::toStr(value);
        auto it = std::find(str.begin(), str.end(), '.');
        if(it != str.end()) {
            size_t offset = 0;
            while(++it != str.end() && offset++ < N) {}
            str.erase(it, str.end());
        }
        return str;
    }
};

template <std::size_t V, std::size_t N, class I = std::make_integer_sequence<std::size_t, N>>
struct power;

template <std::size_t V, std::size_t N, std::size_t... Is>
struct power<V, N, std::integer_sequence<std::size_t, Is...>> {
   static constexpr std::size_t value = (static_cast<std::size_t>(1) * ... * (V * static_cast<bool>(Is + 1)));
};

template<std::size_t N, typename T>
inline auto dec(T value) noexcept {
    if constexpr(N == 0) {
        return decimals_t<N, T>{ T( int64_t( value + 0.5 ) ) };
    }
    constexpr auto D = power<10u, N>::value;
    return decimals_t<N, T>{ T( int64_t( value * D + 0.5 ) ) / D };
}

template<size_t prefix, size_t postfix>
struct TCOLOR {
    static constexpr const char value[] = {
        27, '[',
        '0' + ((prefix - (prefix % 10)) / 10),
        '0' + (prefix % 10),
        ';',
        '0' + ((postfix - (postfix % 10)) / 10),
        '0' + (postfix % 10),
        'm',
        0
    };
};

template<size_t prefix, size_t postfix>
inline constexpr auto COLOR = TCOLOR<prefix, postfix>::value;

template<FormatterType type, FormatColor_t _value>
struct Formatter {
    template<FormatColor_t value = _value>
        requires (type == FormatterType::HTML)
    static constexpr auto tag() noexcept {
        switch (value) {
            case FormatColor::YELLOW:       return "yellow";
            case FormatColor::DARK_RED:     return "darkred";
            case FormatColor::GREEN:        return "green";
            case FormatColor::GRAY:         return "gray";
            case FormatColor::CYAN:         return "cyan";
            case FormatColor::DARK_CYAN:    return "darkcyan";
            case FormatColor::PINK:         return "pink";
            case FormatColor::BLUE:         return "blue";
            case FormatColor::BLACK:        return "\0";
            case FormatColor::WHITE:        return "white";
            case FormatColor::DARK_GRAY:    return "darkgray";
            case FormatColor::LIGHT_GRAY:   return "lightgray";
            case FormatColor::DARK_GREEN:   return "darkgreen";
            case FormatColor::PURPLE:       return "purple";
            case FormatColor::RED:          return "red";
            case FormatColor::DARK_PINK:    return "darkpink";
            case FormatColor::LIGHT_CYAN:   return "lightcyan";
            case FormatColor::ORANGE:       return "orange";
            default:
                return "black";
        }
    }

    template<FormatColor_t value = _value>
        requires (type == FormatterType::TAGS)
    inline static constexpr auto tag() noexcept {
        return value.value();
    }
    
    template<FormatColor_t value = _value>
        requires (type == FormatterType::UNIX)
    static constexpr auto tag() noexcept {
        enum Style {
            none = 0, bold = 1, dark = 2, cursive = 3, underline = 4, blink = 5
        };
        
#if !defined(__EMSCRIPTEN__)
        switch (value) {
            case FormatColor::DARK_YELLOW: return COLOR<dark, 93>;
            case FormatColor::YELLOW: return COLOR<none, 93>;
            case FormatColor::DARK_RED: return COLOR<dark, 91>;
            case FormatColor::GREEN: return COLOR<none, 92>;
            case FormatColor::GRAY: return COLOR<bold, 37>;
            case FormatColor::CYAN: return COLOR<none,96>;
            case FormatColor::DARK_CYAN: return COLOR<none, 36>;
            case FormatColor::PINK: return COLOR<none,95>;
            case FormatColor::BLUE: return COLOR<bold,34>;
    #if defined(WIN32)
            case FormatColor::BLACK: return COLOR<none, 97>;
            case FormatColor::WHITE: return COLOR<bold, 97>;
            case FormatColor::DARK_GRAY: return COLOR<none, 0>;
            case FormatColor::LIGHT_GRAY: return COLOR<none, 37>;
    #else
            case FormatColor::BLACK: return COLOR<none, 0>;
            case FormatColor::WHITE: return COLOR<bold, 0>;
            case FormatColor::DARK_GRAY: return COLOR<none, 90>;
            case FormatColor::LIGHT_GRAY: return COLOR<none, 97>;
    #endif
            case FormatColor::DARK_GREEN: return COLOR<dark, 32>;
            case FormatColor::PURPLE: return COLOR<none, 94>;
            case FormatColor::RED: return COLOR<none, 91>;
            case FormatColor::DARK_PINK: return COLOR<bold, 35>;
            case FormatColor::LIGHT_CYAN: return COLOR<none, 96>;
            case FormatColor::ORANGE: return COLOR<cursive, 33>;
                
            default:
#ifdef WIN32
                return COLOR<none, 97>;
#else
                return COLOR<none, 0>;
#endif
        }
#else
        return "";
#endif
    }

    template<FormatColor_t value = _value>
        requires (type == FormatterType::HTML)
    static constexpr auto tint(const std::string& s) noexcept {
        if(s.empty())
            return std::string();
        if(utils::trim(s).empty())
            return s;
        if constexpr(*tag() == 0)
            return s;
        return std::string("<") + tag() + ">" + s + "</" + tag() + ">";
    }

    template<char value>
    inline static constexpr auto from_code_to_tag() noexcept {
        if constexpr (value >= FormatColor_t::fields().size()) {
            return FormatColor::BLACK.value();
        }
        return FormatColor::data::values( value - 1 );
    }

    template<FormatColor::data::values value, bool end_tag = false>
    inline static constexpr auto from_tag_to_code() noexcept {
        if constexpr (end_tag)
            return char(value) + (char)FormatColor::data::values::INVALID + 1;
        else
            return char(value) + 1;
    }

    template<FormatColor_t value = _value>
        requires (type == FormatterType::TAGS)
    static constexpr auto tint(const std::string& s) noexcept {
        constexpr char buffer[] = { from_tag_to_code<tag()>(), from_tag_to_code<tag(), true>() };
        return std::string(buffer, 1) + s + std::string(buffer + 1, 1);
    }

    template<FormatColor_t value = _value>
        requires (type == FormatterType::UNIX)
    static auto tint(const std::string& s) noexcept {
        static std::once_flag flag;
        static bool dont_print = true;
        std::call_once(flag, []() {
#if __APPLE__
            char* xcode_colors = getenv("OS_ACTIVITY_DT_MODE");
            if (!xcode_colors || (strcmp(xcode_colors, "YES") != 0))
            {
#endif
#if !defined(__EMSCRIPTEN__)
#if defined(WIN32)
                if (_isatty(_fileno(stdout)))
#else
                if (isatty(fileno(stdout)))
#endif
                    dont_print = false;
#endif
#if __APPLE__
            }
#endif
        });

        if(dont_print)
            return s;
        
        return tag() + s + COLOR<0, 0>;
    }
    
    template<FormatColor_t value = _value>
        requires (type == FormatterType::NONE)
    static constexpr auto tint(const std::string& s) noexcept {
        return s;
    }
};

template<FormatColor_t value, FormatterType type>
auto console_color(const std::string& str) noexcept {
    using Formatter_t = Formatter<type, value>;
    return Formatter_t::tint(str);
}

template<typename T, typename U>
concept is_explicitly_convertible =
           std::is_constructible<U, T>::value
        && (!std::is_convertible<T, U>::value);

template<typename T>
concept _has_fmt_color = requires {
    std::same_as<cmn::remove_cvref_t<decltype(T::color)>, cmn::FormatColor_t>;
};

template<FormatterType colors = FormatterType::UNIX>
class ParseValue {
private:
    ParseValue() = delete;
    
public:
    inline static constexpr auto bracket_color = FormatColor::LIGHT_GRAY;
    inline static constexpr auto number_color = FormatColor::GREEN;
    inline static constexpr auto string_color = FormatColor::RED;
    inline static constexpr auto keyword_color = FormatColor::PINK;
    inline static constexpr auto comment_color = FormatColor::GREEN;
    inline static constexpr auto function_color = FormatColor::YELLOW;
    inline static constexpr auto class_color = FormatColor::CYAN;
    
    /*inline static constexpr auto bracket_color = FormatColor::GRAY;
    inline static constexpr auto number_color = FormatColor::CYAN;
    inline static constexpr auto string_color = FormatColor::RED;
    inline static constexpr auto keyword_color = FormatColor::DARK_CYAN;
    inline static constexpr auto comment_color = FormatColor::DARK_CYAN;
    inline static constexpr auto function_color = FormatColor::GREEN;
    inline static constexpr auto class_color = FormatColor::YELLOW;*/
    
    template<typename T>
        requires _has_tostr_method<T> && _has_fmt_color<T>
    static std::string parse_value(const T& value) {
        return console_color<T::color, colors>(value.toStr());
    }
    
    template<typename T, typename K = std::remove_cvref_t<T>>
        requires _has_tostr_method<K>
                && (!_has_fmt_color<T>)
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
        return console_color<string_color, colors>(Meta::toStr(value));
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
            COMMENT = 7, MULTI_COMMENT, NAMESPACE, ESCAPED_CHAR, TAG_WHITESPACE
        };

        std::vector<State> states{ NONE };

        auto is_state = [&](State s) {
            return (states.back() == s);
        };

        auto is_keyword = [](std::string word) {
            word = utils::trim(utils::lowercase(word));
            static const std::vector<std::string> keywords{
                //"string", "pv",
                //"path",
                //"array", "map", "set", "pair",
                "int", "float", "double", "bool", "byte", "char", "unsigned", "int64_t", "uint64_t", "int32_t", "uint32_t", "int16_t", "uint16_t", "int8_t", "uint8_t", "uint",
                "true", "false",
                "if", "def"
            };
            return contains(keywords, word);
        };

        /*auto alpha_numeric = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        };*/

        auto apply_state = [&](State s) {
            if (!tmp.empty()) {
                switch (s) {
                case MULTI_COMMENT:
                    //printf("MULTI COMMENT %s\n", tmp.c_str());
                    ss << console_color<comment_color, colors>(tmp);
                    break;
                case COMMENT:
                    //printf("COMMENT %s\n", tmp.c_str());
                    ss << console_color<comment_color, colors>(tmp);
                    break;
                case ESCAPED_CHAR:
                    //printf("ESCAPED %s\n", tmp.c_str());
                    ss << console_color<FormatColor::DARK_RED, colors>(tmp);
                    break;
                case STRING:
                    //printf("STRING %s\n", tmp.c_str());
                    ss << console_color<string_color, colors>(tmp);
                    break;
                case NUMBER:
                    //printf("NUMBER %s\n", tmp.c_str());
                    ss << console_color<number_color, colors>(tmp);
                    break;
                case TAG_WHITESPACE:
                    ss << console_color<FormatColor::LIGHT_GRAY, colors>(tmp);
                    break;
                case CLASS:
                    //printf("CLASS %s\n", tmp.c_str());
                    ss << console_color<class_color, colors>(tmp);
                    break;
                case NAMESPACE:
                    ss << console_color<FormatColor::DARK_CYAN, colors>(tmp);
                    break;
                case FUNCTION:
                    //printf("FUNCTION: %s\n", tmp.c_str());
                    if (!is_keyword(tmp))
                        ss << console_color<function_color, colors>(tmp);
                    else
                        ss << console_color<keyword_color, colors>(tmp);
                    break;
                case TAG:
                    //printf("TAG %s\n", tmp.c_str());
                    if (!is_keyword(tmp))
                        ss << console_color<FormatColor::CYAN, colors>(tmp);
                    else
                        ss << console_color<keyword_color, colors>(tmp);
                    break;
                case WHITESPACE: {
                    if (tmp == "::") {
                        //printf("TAG:: %s\n", tmp.c_str());
                        ss << console_color<FormatColor::LIGHT_GRAY, colors>(tmp);
                    }
                    else {
                        //printf("WHITESPACE: %s\n", tmp.c_str());
                        ss << console_color<FormatColor::BLACK, colors>(tmp);
                    }
                    break;
                }

                default:
                    if (is_keyword(tmp)) {
                        ss << console_color<keyword_color, colors>(tmp);
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
            
            if (is_state(NUMBER)) {
                if (s[i] == '.' || (s[i] >= '0' && s[i] <= '9') || s[i] == 'f') {
                }
                else
                    pop_state();
            }
            
            if (is_state(TAG) && s[i] == '>') {
                //printf("is_state TAG : %c\n", s[i]);
                pop_state();
                push_state(TAG_WHITESPACE); tmp += s[i]; pop_state();
                continue;
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
            else if (s[i] == '"' || s[i] == '\'' || s[i] == '`') {
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
                push_state(TAG_WHITESPACE); tmp += s[i]; pop_state();
                push_state(TAG);
                continue;
            }
            else if ((i == 0 || (utils::contains(whitespace, s[i-1]) && s[i-1] != '.')) && !is_state(NUMBER) && s[i] >= '0' && s[i] <= '9') {
                push_state(NUMBER);
            }
            else if ((!is_state(NUMBER) || s[i] != '.') &&  utils::contains(whitespace, s[i])) {
                if (!is_state(WHITESPACE)) {
                    while (is_state(CLASS) || is_state(NAMESPACE) || is_state(FUNCTION)) {
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
        return pretty_text(std::string(s));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires _is_dumb_pointer<T> && std::same_as<T, const char*>
    static std::string parse_value(T s) {
        return pretty_text(s);
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires _is_smart_pointer<K>
    static std::string parse_value(const T& s) {
        return "ptr<"+ console_color<class_color, colors>( Meta::name<typename K::element_type>() ) + ">"
        + (s ? parse_value(*s) : console_color<keyword_color, colors>("null"));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires (!std::same_as<bool, K>)
            && _is_number<K>
    static std::string parse_value(T && value) {
        return console_color<number_color, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::same_as<bool, K>
    static std::string parse_value(T value) {
        return console_color<keyword_color, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::is_enum<K>::value
    static std::string parse_value(T value) {
        return console_color<number_color, colors>(Meta::toStr<int>((int)value));
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires is_instantiation<cv::Size_, T>::value
    static std::string parse_value(T value) {
        return console_color<bracket_color, colors>("[")
            + parse_value(value.width)
            + console_color<bracket_color, colors>(",")
            + parse_value(value.height)
            + console_color<bracket_color, colors>("]");
    }

    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::same_as<K, std::string>
    static std::string parse_value(const T& value) {
        return console_color<string_color, colors>(Meta::toStr(value));
    }
    
            
    template<class TupType, size_t... I>
    static std::string tuple_str(const TupType& _tup, std::index_sequence<I...>)
    {
                
        std::stringstream str;
        str << console_color<bracket_color, colors>("[");
        (..., (str << (I == 0? "" : console_color<FormatColor::BLACK, colors>(",")) << parse_value(std::get<I>(_tup))));
        str << console_color<bracket_color, colors>("]");
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
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires std::is_base_of<std::exception, K>::value
    static std::string parse_value(const T& value) {
        return console_color<class_color, colors>(Meta::name<K>())
            + console_color<FormatColor::BLACK, colors>("<")
            + (value.what()
               ? console_color<string_color, colors>("\""+std::string(value.what())+"\"")
               : console_color<FormatColor::DARK_GRAY, colors>("unknown"))
            + console_color<FormatColor::BLACK, colors>(">") ;
    }

    template<template <typename, typename> class T, typename A, typename B>
        requires is_pair<T<A, B>>::value
    static std::string parse_value(const T<A, B>& m) {
        return console_color<bracket_color, colors>("[")
            + parse_value(m.first)
            + console_color<bracket_color, colors>(",")
            + parse_value(m.second)
            + console_color<bracket_color, colors>("]");
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
        std::string str = console_color<bracket_color, colors>("[");
        auto it = m.begin(), end = m.end();
        if (it != end) {
            for (;;) {
                str += parse_value(*it);

                if (++it != end)
                    str += console_color<bracket_color, colors>(",");
                else
                    break;
            }
        }
        return str + console_color<bracket_color, colors>("]");
    }

    template<bool A, size_t S, typename... Args>
    static std::string parse_value(const robin_hood::detail::Table<A, S, Args...>& m) {
        size_t i = 0, N = m.size();
        std::string str = console_color<bracket_color, colors>("{");
        for (auto& [k, v] : m) {
            str += console_color<bracket_color, colors>("[")
                + parse_value(k)
                + console_color<bracket_color, colors>(":")
                + parse_value(v)
                + console_color<bracket_color, colors>(i++ < N-1 ? "]," : "]");
        }
        return str + console_color<bracket_color, colors>("}");
    }
    
    template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
        requires is_map<K>::value
    static std::string parse_value(const T<Args...>& m) {
        std::string str = console_color<bracket_color, colors>("{");
        size_t i = 0, N = m.size();
        for (auto& [k, v] : m) {
            str += console_color<bracket_color, colors>("[")
                + parse_value(k)
                + console_color<bracket_color, colors>(":")
                + parse_value(v)
                + console_color<bracket_color, colors>(i++ < N-1 ? "]," : "]");
        }
        return str + console_color<bracket_color, colors>("}");
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
            return str + console_color<keyword_color, colors>("null");
        //if constexpr (_has_tostr_method<std::remove_pointer_t<T>>)
        //    return str + parse_value(*ptr);

        std::stringstream stream;
        stream << "0x" << std::hex << (uint64_t)ptr;
        return str + console_color<FormatColor::PURPLE, colors>(stream.str());
    }
};

namespace fmt {
    template<cmn::FormatColor_t _color, typename T>
    struct _clr {
        static constexpr cmn::FormatColor_t color = _color;
        using value_type = T;
        
        const T& obj;
        _clr(const T& obj) : obj(obj) { }
        
        //operator const T&() const { return obj; }
        std::string toStr() const {
            if constexpr(std::convertible_to<T, std::string>) {
                return (std::string)obj;
            } else
                return Meta::toStr(obj);
        }
    };

    template<cmn::FormatColor_t _color, typename T>
    auto clr(const T& obj) -> _clr<_color, T> { return { obj }; }

    template<typename T> auto red(const T& obj) -> _clr<FormatColor::RED, T> { return { obj }; }
    template<typename T> auto green(const T& obj) -> _clr<FormatColor::GREEN, T> { return { obj }; }
    template<typename T> auto yellow(const T& obj) -> _clr<FormatColor::YELLOW, T> { return { obj }; }
    template<typename T> auto cyan(const T& obj) -> _clr<FormatColor::CYAN, T> { return { obj }; }
    template<typename T> auto black(const T& obj) -> _clr<FormatColor::BLACK, T> { return { obj }; }
}

template<FormatterType type, typename... Args>
std::string format(const Args& ... args) {
    return (... + (ParseValue<type>::parse_value(args)));
}

#if COMMONS_FORMAT_LOG_TO_FILE
void write_log_message(const std::string&);
void set_log_file(const std::string&);
bool has_log_file();
#endif

void log_to_terminal(const std::string&, bool force_display = false);
void log_to_callback(const std::string&, PrefixLiterals::Prefix = PrefixLiterals::Prefix::INFO, bool force = false);
void set_runtime_quiet(bool);
void* set_debug_callback(std::function<void(PrefixLiterals::Prefix, const std::string&, bool)>);
bool has_log_callback();

inline std::string current_time_string() {
    using namespace std::chrono;
    static const auto tod = ([](){
        time_t t = time(NULL);
        struct tm lt;
        memset(&lt, 0, sizeof(tm));
        localtime_r(&t, &lt);
        return std::chrono::seconds(lt.tm_gmtoff);
    })();
    auto t = date::floor<seconds>(system_clock::now() + tod);
    return date::format("%H:%M:%S", t);
}

template<typename... Args>
void print(const Args & ... args) {
    static constexpr auto bracket_color = ParseValue<FormatterType::UNIX>::bracket_color;
    
    auto str =
        console_color<bracket_color, FormatterType::UNIX>( "[" )
        + console_color<FormatColor::CYAN, FormatterType::UNIX>( current_time_string() )
        + console_color<bracket_color, FormatterType::UNIX>( "]" ) + " "
        + format<FormatterType::UNIX>(args...);
    
    log_to_terminal(str);

#if COMMONS_FORMAT_LOG_TO_FILE
    if (has_log_file()) {
        str = "<row>" + console_color<bracket_color, FormatterType::HTML>("[")
            + console_color<FormatColor::CYAN, FormatterType::HTML>(current_time_string())
            + console_color<bracket_color, FormatterType::HTML>("] ") + format<FormatterType::HTML>(args...) + "</row>";
        write_log_message(str);
    }
#endif
    if (has_log_callback())
        log_to_callback(format<FormatterType::NONE>(args...));
}

}

#include <misc/utilsexception.h>

namespace cmn {

template<FormatterType formatter, PrefixLiterals::Prefix prefix, FormatColor_t color, typename... Args>
struct FormatColoredPrefix {
    FormatColoredPrefix(const Args& ...args, cmn::source_location info = cmn::source_location::current()) {
        static constexpr auto bracket_color = ParseValue<formatter>::bracket_color;
        
        auto universal = utils::find_replace(info.file_name(), "\\", "/");
        auto split = utils::split(universal, '/');
        auto &file = split.empty() ? universal : split.back();
        
        std::string str =
          console_color<bracket_color, formatter>("[")
          + cmn::console_color<color, formatter>(
                std::string(PrefixLiterals::names[(size_t)prefix]) + " " + cmn::current_time_string()
                + " " + file + ":" + std::to_string(info.line()))
          + console_color<bracket_color, formatter>("]") + " "
          + format<formatter>(args...);
        
        log_to_terminal(str);

#if COMMONS_FORMAT_LOG_TO_FILE
        if (has_log_file()) {
            str = "<row>" + console_color<bracket_color, FormatterType::HTML>("[")
                + console_color<FormatColor::CYAN, FormatterType::HTML>(current_time_string())
                + console_color<bracket_color, FormatterType::HTML>("] ") + format<FormatterType::HTML>(args...) + "</row>\n";
            write_log_message(str);
        }
#endif
        if (has_log_callback())
            log_to_callback(format<FormatterType::NONE>(args...), prefix);
    }
};

template<typename... Args>
struct FormatWarning : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::WARNING, FormatColor::YELLOW, Args...> {
    FormatWarning(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::WARNING, FormatColor::YELLOW, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatWarning(const Args&... args) -> FormatWarning<Args...>;

template<typename... Args>
struct FormatError : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::ERROR_PREFIX, FormatColor::RED, Args...> {
    FormatError(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::ERROR_PREFIX, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatError(const Args&... args) -> FormatError<Args...>;

static constexpr const char EXCEPTION_PREFIX[] = "EXCEPT";
template<typename... Args>
struct FormatExcept : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...> {
    FormatExcept(const Args& ...args, cmn::source_location info = cmn::source_location::current())
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
FormatExcept(const Args&... args) -> FormatExcept<Args...>;

template<FormatterType formatter, typename... Args>
struct U_EXCEPTION : UtilsException {
    U_EXCEPTION(const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : UtilsException(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<formatter, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
};

template<FormatterType formatter = FormatterType::UNIX, typename... Args>
U_EXCEPTION(const Args& ...args) -> U_EXCEPTION<formatter, Args...>;

class SoftExceptionImpl : public std::exception {
public:
    template<typename... Args>
    SoftExceptionImpl(cmn::source_location info, const Args& ...args)
        : msg(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
    
    ~SoftExceptionImpl() throw() {
    }

    virtual const char * what() const throw() {
        return msg.c_str();
    }

private:
    std::string msg;
};

template< typename... Args>
struct SoftException : SoftExceptionImpl {
    SoftException(const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : SoftExceptionImpl(info, args...)
    {
    }
};


template<typename... Args>
SoftException(Args... args) -> SoftException<Args...>;

template <typename T> struct type_t {};
template <typename T> inline type_t<T> type{};

template< class T, typename... Args>
struct CustomException : T {
    CustomException(type_t<T>, const Args& ...args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : T(cmn::format<FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info);
    }
};


template<typename T, typename... Args>
CustomException(type_t<T>, Args... args) -> CustomException<T, Args...>;

template<typename... Args>
void DebugHeader(const Args& ...args) {
    std::string str = format<FormatterType::NONE>(args...);
    auto line = utils::repeat("-", str.length());
    print(line.c_str());
    print(args...);
    print(line.c_str());
}


template<typename... Args>
void DebugCallback(const Args& ...args) {
    print<Args...>(args...);
}


namespace Meta {
template<> inline std::string name<SoftExceptionImpl>() {
    return "SoftException";
}

template<> inline std::string name<UtilsException>() {
    return "UtilsException";
}
}

}
