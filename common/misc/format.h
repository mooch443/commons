#pragma once
#include <commons.pc.h>
#include <misc/FormatColor.h>
#include <misc/utilsexception.h>

#if defined(WIN32)
#include <io.h>
#endif

#define COMMONS_FORMAT_LOG_TO_FILE true

namespace cmn {

namespace print_colors {
    inline static std::once_flag flag;
    inline static bool dont_print = true;
}

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

template<utils::StringLike Str>
struct no_quotes_t {
    Str str;
    std::string toStr() const {
        return (std::string)str;
    }
};

template<utils::StringLike T>
inline auto no_quotes(T&& str) noexcept {
    return no_quotes_t<T>{ str };
}

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
        return value;//.value();
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

    /*template<char value>
    inline static constexpr auto from_code_to_tag() noexcept {
        if constexpr (value >= FormatColor_t::fields().size()) {
            return FormatColor::BLACK.value();
        }
        return FormatColor::data::values( value - 1 );
    }*/

    template<FormatColor value, bool end_tag = false>
    inline static constexpr auto from_tag_to_code() noexcept {
        if constexpr (end_tag)
            return char(value) + (char)FormatColor::INVALID + 1;
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
        std::call_once(print_colors::flag, []() {
#if __APPLE__
            auto TERM = getenv("TERM");
            if(not TERM || strcmp(TERM, "dumb") == 0) {
                // Terminal doesn't support ANSI color codes
                //printf("Probably does not support ANSI color codes.\n");
                return;
            }
            
            char* xcode_colors = getenv("__XCODE_BUILT_PRODUCTS_DIR_PATHS");
            if (not xcode_colors)
            {
#endif
#if !defined(__EMSCRIPTEN__)
#if defined(WIN32)
                if (_isatty(_fileno(stdout)))
#else
                if (isatty(fileno(stdout)))
#endif
                    print_colors::dont_print = false;
#endif
#if __APPLE__
            }
#endif
        });

        if(print_colors::dont_print)
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
    requires std::same_as<cmn::remove_cvref_t<decltype(T::color)>, cmn::FormatColor_t>;
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
        requires (std::convertible_to<K, std::string> 
                  || is_explicitly_convertible<K, std::string>)
                && (!std::same_as<K, std::string>)
                && (!are_the_same<K, std::string_view>)
                && (!_is_dumb_pointer<T>)
    static std::string parse_value(const T& value) {
        return console_color<string_color, colors>(Meta::toStr(value));
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires are_the_same<K, std::string_view>
    static std::string parse_value(const T& value) {
        return pretty_text(value);
    }

    template<utils::StringLike Str>
    static std::string pretty_text(Str&& s) {
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

        auto is_keyword = [](std::string_view word) {
            auto trimmed = utils::lowercase(utils::trim(word));
            static constexpr std::array<std::string_view, 20> keywords{
                //"string", "pv",
                //"path",
                //"array", "map", "set", "pair",
                "int", "float", "double", "bool", "byte", "char", "unsigned", "int64_t", "uint64_t", "int32_t", "uint32_t", "int16_t", "uint16_t", "int8_t", "uint8_t", "uint",
                "true", "false",
                "if", "def"
            };
            return contains(keywords, trimmed);
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

        std::string_view sv(s);
        const size_t N = sv.size();
        char str_char = 0;

        for (size_t i = 0; i < N; ++i) {
            if (not sv[i])
                break;

            if (is_state(WHITESPACE)) {
                if (i == 0 || sv[i] != sv[i - 1])
                    pop_state();
            }
            
            if (is_state(NUMBER)) {
                if (sv[i] == '.' || (sv[i] >= '0' && sv[i] <= '9') || sv[i] == 'f') {
                }
                else
                    pop_state();
            }
            
            if (is_state(TAG) && sv[i] == '>') {
                //printf("is_state TAG : %c\n", s[i]);
                pop_state();
                push_state(TAG_WHITESPACE); tmp += sv[i]; pop_state();
                continue;
            }

            if (is_state(COMMENT) || is_state(MULTI_COMMENT)) {
                if (is_state(COMMENT) && sv[i] == '\n') {
                    pop_state();
                }
                else if (is_state(MULTI_COMMENT) && sv[i] == '*' && i < N - 1 && sv[i + 1] == '/') {
                    tmp += sv[i];
                    tmp += sv[i + 1];
                    ++i;
                    pop_state();
                    continue;
                }

            }
            else if (sv[i] == '"' || sv[i] == '\'' || sv[i] == '`') {
                if (!is_state(STRING)) {
                    str_char = sv[i];
                    push_state(STRING);
                }
                else if (sv[i] == str_char) {
                    tmp += sv[i];
                    pop_state();
                    continue;
                }
            }
            else if (is_state(STRING)) {
                if (sv[i] == '\\' && i < N - 1) //&& (alpha_numeric(s[i + 1]) || ))
                {
                    push_state(ESCAPED_CHAR);
                    tmp += sv[i];
                    tmp += sv[i + 1];
                    ++i;
                    pop_state();
                    continue;
                }
            }
            else if (!is_state(FUNCTION) && sv[i] == '(') {
                if (states.back() == NAMESPACE || states.back() == CLASS) {
                    states.back() = FUNCTION;
                    pop_state();
                }
                else {
                    states.push_back(FUNCTION);
                    pop_state();
                }

                push_state(WHITESPACE); tmp += sv[i]; pop_state();
                continue;
            }
            else if (!is_state(COMMENT) && sv[i] == '/' && i < N - 1 && sv[i + 1] == '/') {
                push_state(COMMENT);
            }
            else if (!is_state(MULTI_COMMENT) && sv[i] == '/' && i < N - 1 && sv[i + 1] == '*') {
                push_state(MULTI_COMMENT);
            }
            else if (sv[i] == '<') {
                states.push_back(CLASS); pop_state();
                push_state(TAG_WHITESPACE); tmp += sv[i]; pop_state();
                push_state(TAG);
                continue;
            }
            else if ((i == 0 || (utils::contains(whitespace, sv[i-1]) && sv[i-1] != '.')) && !is_state(NUMBER) && sv[i] >= '0' && sv[i] <= '9') {
                push_state(NUMBER);
            }
            else if ((!is_state(NUMBER) || sv[i] != '.') &&  utils::contains(whitespace, sv[i])) {
                if (!is_state(WHITESPACE)) {
                    while (is_state(CLASS) || is_state(NAMESPACE) || is_state(FUNCTION)) {
                        pop_state();
                    }

                    if (sv[i] == ':' && i < N - 1 && sv[i + 1] == sv[i]) {
                        states.push_back(NAMESPACE);
                        pop_state();
                        push_state(WHITESPACE); tmp += sv[i]; tmp += sv[i + 1]; pop_state();
                        push_state(CLASS);
                        ++i;
                        continue;
                    }

                    push_state(WHITESPACE);
                }
            }

            tmp += sv[i];
        }

        if (!tmp.empty())
            pop_state();

        return ss.str();
    }

    template<typename T, size_t N, typename K = cmn::remove_cvref_t<T>>
        requires std::convertible_to<T[N], std::string_view>
    static std::string parse_value(T(&s)[N]) {
        return pretty_text(std::string_view(s, N));
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
    
    
    template<template <typename ...> class T, typename Rt, typename... Args>
        requires is_instantiation<std::function, T<Rt(Args...)>>::value
    static std::string parse_value(const T<Rt(Args...)>&) {
        std::stringstream ss;
        bool first = true;
        ((ss << (first ? "" : ", ") << Meta::name<Args>(), first = false), ...);
        return "function<"+Meta::name<Rt>()+"("+ss.str()+")>";
    }
    
    template<typename T, typename K = cmn::remove_cvref_t<T>>
        requires is_set<K>::value || is_container<K>::value
    static std::string parse_value(const T&m) {
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

    template<template <typename...> class T, typename... Args, typename K = cmn::remove_cvref_t<T<Args...>>>
        requires is_queue<K>::value
    static std::string parse_value(const T<Args...>&m) {
        std::string str = console_color<bracket_color, colors>("[");
        auto copy = m;
        size_t i=0;
        while(!copy.empty()) {
            auto &it = copy.front();
            if(i > 0)
                str += console_color<bracket_color, colors>(",");
            str += parse_value(it);
            copy.pop();
            ++i;
        }
        return str + console_color<bracket_color, colors>("]");
    }
    
    template<bool A, size_t S, typename Key, typename T, typename... Args>
    static std::string parse_value(const robin_hood::detail::Table<A, S, Key, T, Args...>& m) {
        size_t i = 0, N = m.size();
        std::string str = console_color<bracket_color, colors>("{");
        if constexpr(std::same_as<T, void>) {
            for (auto& v : m) {
                str += parse_value(v)
                        + console_color<bracket_color, colors>(i++ < N-1 ? "," : "");
            }
        } else {
            for (auto& [k, v] : m) {
                str +=
                    parse_value(k)
                    + console_color<bracket_color, colors>(":")
                    + parse_value(v)
                    + (i++ < N-1 ? console_color<bracket_color, colors>(",") : "");
            }
        }
        return str + console_color<bracket_color, colors>("}");
    }
    
    template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
        requires is_map<K>::value
    static std::string parse_value(const T<Args...>& m) {
        std::string str = console_color<bracket_color, colors>("{");
        size_t i = 0, N = m.size();
        for (auto& [k, v] : m) {
            str +=
                parse_value(k)
                + console_color<bracket_color, colors>(":")
                + parse_value(v)
                + (i++ < N-1 ? console_color<bracket_color, colors>(",") : "");
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
        if constexpr(_has_tostr_method<std::remove_pointer_t<T>>) {
            stream << ptr->toStr();
        } else {
            stream << "0x" << std::hex << (uint64_t)ptr;
        }
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

template<typename T>
    requires std::integral<T> || std::is_pointer_v<T>
struct hex_t {
    static constexpr cmn::FormatColor_t color = ParseValue<>::keyword_color;
    
    T value;
    std::string toStr() const {
        std::stringstream ss;
        ss << std::hex << value;
        return ss.str();
    }
};

template<typename T>
inline auto hex(T value) noexcept {
    return hex_t<T>{ value };
}

template<FormatterType type, typename... Args>
std::string format(const Args& ... args) {
    return (... + (ParseValue<type>::parse_value(args)));
}
template<FormatterType type>
std::string format() {
    return ParseValue<type>::parse_value("");
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

#if _WIN32
#    define timegm _mkgmtime
#endif

inline std::string current_time_string() {
    using namespace std::chrono;
    static const auto tod = ([](){
        time_t t = time(NULL);
        struct tm buf;
#ifdef _WIN32
        localtime_s(&buf, &t);
#else
        localtime_r(&t, &buf);
#endif
        struct tm locl;
        memcpy(&locl, &buf, sizeof(struct tm));
        return std::chrono::seconds(timegm(&buf) - mktime(&locl));
    })();
    auto t = date::floor<seconds>(system_clock::now() + tod);
    return date::format("%H:%M:%S", t);
}

template<typename... Args>
void Print(const Args & ... args) {
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

template<utils::StringLike Prefix, typename... Args>
void prefixed_print(Prefix&& prefix, const Args & ... args) {
    static constexpr auto bracket_color = ParseValue<FormatterType::UNIX>::bracket_color;
    
    auto str =
        console_color<bracket_color, FormatterType::UNIX>( "[" )
        + console_color<FormatColor::CYAN, FormatterType::UNIX>( current_time_string() )
        + " "
        + console_color<FormatColor::YELLOW, FormatterType::UNIX>( prefix )
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

template<typename... Args>
void thread_print(const Args & ... args) {
    static constexpr auto bracket_color = ParseValue<FormatterType::UNIX>::bracket_color;
    
    auto str =
        console_color<bracket_color, FormatterType::UNIX>( "[" )
        + console_color<FormatColor::CYAN, FormatterType::UNIX>( current_time_string() )
        + " "
        + console_color<FormatColor::YELLOW, FormatterType::UNIX>( get_thread_name() )
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

namespace cmn {

template<FormatterType formatter, PrefixLiterals::Prefix prefix, FormatColor_t color, typename... Args>
struct FormatColoredPrefix {
    FormatColoredPrefix(const Args& ...args, cmn::source_location info = cmn::source_location::current()) {
        static constexpr auto bracket_color = ParseValue<formatter>::bracket_color;
        
        auto universal = utils::find_replace(info.file_name(), "\\", "/");
        auto split = utils::split(universal, '/');
        auto file = split.empty() ? universal : split.back();
        
        std::string str =
          console_color<bracket_color, formatter>("[")
          + cmn::console_color<color, formatter>(
                std::string(PrefixLiterals::names[(size_t)prefix]) + " " + cmn::current_time_string()
                + " " + (std::string)file + ":" + std::to_string(info.line()))
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
struct _FormatWarning : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::WARNING, FormatColor::YELLOW, Args...> {
    _FormatWarning(cmn::source_location info, const Args& ...args)
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::WARNING, FormatColor::YELLOW, Args...>(args..., info)
    { }
};

template<typename... Args>
_FormatWarning(const Args&... args) -> _FormatWarning<Args...>;

#define FormatWarning(...) _FormatWarning(cmn::source_location::current(), __VA_ARGS__)

template<typename... Args>
struct _FormatError : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::ERROR_PREFIX, FormatColor::RED, Args...> {
    _FormatError(cmn::source_location info, const Args& ...args)
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::ERROR_PREFIX, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
_FormatError(const Args&... args) -> _FormatError<Args...>;

#define FormatError(...) _FormatError(cmn::source_location::current(), __VA_ARGS__)

static constexpr const char EXCEPTION_PREFIX[] = "EXCEPT";
template<typename... Args>
struct _FormatExcept : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...> {
    _FormatExcept(cmn::source_location info, const Args& ...args)
        : FormatColoredPrefix<FormatterType::UNIX, PrefixLiterals::EXCEPT, FormatColor::RED, Args...>(args..., info)
    { }
};

template<typename... Args>
_FormatExcept(const Args&... args) -> _FormatExcept<Args...>;

#define FormatExcept(...) _FormatExcept(cmn::source_location::current(), __VA_ARGS__)

template<typename... Args>
void DebugHeader(const Args& ...args) {
    std::string str = format<FormatterType::NONE>(args...);
    auto line = utils::repeat("-", str.length());
    Print(line.c_str());
    Print(args...);
    Print(line.c_str());
}


template<typename... Args>
void DebugCallback(const Args& ...args) {
    Print<Args...>(args...);
}

class UtilsException : public std::exception {
public:

    UtilsException(const std::string& str);

    ~UtilsException() throw();

    virtual const char * what() const throw();

private:
    std::string msg;
};

template<FormatterType formatter, typename... Args>
struct _U_EXCEPTION : cmn::UtilsException {
    _U_EXCEPTION(cmn::source_location info, const Args& ...args) noexcept(false)
        : cmn::UtilsException(cmn::format<cmn::FormatterType::NONE>(args...))
    {
        FormatColoredPrefix<formatter, cmn::PrefixLiterals::EXCEPT, cmn::FormatColor::RED, Args...>(args..., info);
    }
};

template<cmn::FormatterType formatter = cmn::FormatterType::UNIX, typename... Args>
_U_EXCEPTION(cmn::source_location loc, const Args& ...args) -> _U_EXCEPTION<formatter, Args...>;

#define U_EXCEPTION(...) _U_EXCEPTION(cmn::source_location::current(), __VA_ARGS__)

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
struct _SoftException : SoftExceptionImpl {
    _SoftException(cmn::source_location info, const Args& ...args) noexcept(false)
    : SoftExceptionImpl(info, args...)
    { }
};

template<typename... Args>
_SoftException(cmn::source_location loc, const Args& ...args) -> _SoftException<Args...>;

#define SoftException(...) _SoftException(cmn::source_location::current(), __VA_ARGS__)

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
struct InvalidArgumentException : CustomException<std::invalid_argument, Args...> {
    InvalidArgumentException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : CustomException<std::invalid_argument, Args...>(type_t<std::invalid_argument>{}, args..., info)
    { }
};

template<typename... Args>
InvalidArgumentException(Args... args) -> InvalidArgumentException<Args...>;

template<typename... Args>
struct RuntimeError : CustomException<std::runtime_error, Args...> {
    RuntimeError(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : CustomException<std::runtime_error, Args...>(type_t<std::runtime_error>{}, args..., info)
    { }
};

template<typename... Args>
RuntimeError(Args... args) -> RuntimeError<Args...>;

template<typename... Args>
struct InvalidSyntaxException : CustomException<std::runtime_error, Args...> {
    InvalidSyntaxException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : CustomException<std::runtime_error, Args...>(type_t<std::runtime_error>{}, args..., info)
    { }
};

template<typename... Args>
InvalidSyntaxException(Args... args) -> InvalidSyntaxException<Args...>;

template<typename... Args>
struct OutOfRangeException : CustomException<std::out_of_range, Args...> {
    OutOfRangeException(const Args&... args, cmn::source_location info = cmn::source_location::current()) noexcept(false)
        : CustomException<std::out_of_range, Args...>(type_t<std::out_of_range>{}, args..., info)
    { }
};

template<typename... Args>
OutOfRangeException(Args... args) -> OutOfRangeException<Args...>;

namespace Meta {
template<> inline std::string name<SoftExceptionImpl>() {
    return "SoftException";
}

template<> inline std::string name<UtilsException>() {
    return "UtilsException";
}
}

}
