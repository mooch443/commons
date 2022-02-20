#include <gui/IMGUIBase.h>
#include <misc/CommandLine.h>
#include <misc/PVBlob.h>
using namespace gui;

namespace cexpression
{
    double constexpr sqrtNewtonRaphson(double x, double curr, double prev)
    {
        return curr == prev
            ? curr
            : sqrtNewtonRaphson(x, 0.5 * (curr + x / curr), curr);
    }

    double constexpr sqrt(double x)
    {
        return x >= 0 && x < std::numeric_limits<double>::infinity()
            ? sqrtNewtonRaphson(x, x, 0)
            : std::numeric_limits<double>::quiet_NaN();
    }
}

struct Pixel {
    Vec2 offset;
    Vec2 pos;
    Color color;
};

static constexpr unsigned char owl[]{
    " * * * * * * * * *"
    "  *     * *     * "
    " *   *   *   *   *"
    "    * *     * *   "
    " *   *       *   *"
    "  *     * *     * "
    " * *     *     * *"
    "  * *         *   "
    " * * * * * * *   *"
    "  * * * *         "
    " * * * * *       *"
    "  * * * *         "
    "   * * * *       *"
    "    * * * *       "
    "     * * * *     *"
    "      * * * *     "
    "       * * * *   *"
    "        *   * *   "
    "       *   *   * *"
    "  * * * * * *   * "
    "                 *"
};

static constexpr float radius = 5;
static float dt = 1;
static Pixel offset[sizeof(owl)];
static Vec2 target;

template<size_t i>
constexpr bool is_pixel() {
    return owl[i] == '*';
}

static Entangled e;

template<size_t i>
constexpr void action(DrawStructure& graph) {
    if constexpr (!is_pixel<i>())
        return;
    graph.circle(offset[i].pos, radius + 3, White.alpha(200), offset[i].color);
}

template<size_t i>
constexpr Vec2 get_start() {
    return  Vec2{ (i % 18) * radius * 2, (i / 18) * radius * 2 };
}

template<size_t i>
constexpr void inc_offset() {
    if constexpr (!is_pixel<i>())
        return;
    constexpr auto start = get_start<i>();
    offset[i].pos += ((target + start - radius * 18) - offset[i].pos) * dt 
                         * ((start - 18 * radius).length() / 100 + 1);
}

template<size_t i>
constexpr void init_offset() {
    if constexpr (!is_pixel<i>())
        return;
    constexpr Vec2 start = Vec2{ (i % 18) * radius * 2, (i / 18) * radius * 2 };
    constexpr auto f = cexpression::sqrt(start.x * start.x + start.y * start.y) / 300;
    offset[i].offset = offset[i].pos = start;
    offset[i].color = Color{
        static_cast<uint8_t>(255),
        static_cast<uint8_t>(125 * f),
        static_cast<uint8_t>(125 * (1 - f)),
        255u
    };
}

#if defined(WIN32)
#include <io.h>
#endif

enum class CONSOLE_COLORS{
    BLACK = 0, DARK_BLUE = 1, DARK_GREEN = 2, DARK_CYAN = 3, DARK_RED = 4, PURPLE = 5, DARK_YELLOW = 6,
    GRAY = 7, DARK_GRAY = 8, BLUE = 9, GREEN = 10, CYAN = 11, RED = 12, PINK = 13, YELLOW = 14,
    WHITE = 15, LIGHT_BLUE, ORANGE
};

enum class COLOR_TYPE {
    UNIX,
    TAGS,
    HTML
};

template<CONSOLE_COLORS value, COLOR_TYPE type = COLOR_TYPE::UNIX>
    requires (type == COLOR_TYPE::UNIX)
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
        return "";
    
#define COLOR(PREFIX, FINAL) { prefix = PREFIX; final = FINAL; break; }
    int final = 0, prefix = 22;
    
    switch (value) {
        case CONSOLE_COLORS::YELLOW: COLOR(1, 33)
        case CONSOLE_COLORS::DARK_RED: COLOR(2, 91)
        case CONSOLE_COLORS::GREEN: COLOR(1, 32)
    #if defined(__APPLE__)
        case CONSOLE_COLORS::GRAY: COLOR(1, 30)
    #elif defined(WIN32)
        case CONSOLE_COLORS::GRAY: COLOR(1, 90)
    #else
        case CONSOLE_COLORS::GRAY: COLOR(0, 37)
    #endif
        case CONSOLE_COLORS::CYAN: COLOR(1,36)
        case CONSOLE_COLORS::DARK_CYAN: COLOR(0, 36)
        case CONSOLE_COLORS::PINK: COLOR(0,35)
        case CONSOLE_COLORS::BLUE: COLOR(1,34)
#if defined(WIN32)
        case CONSOLE_COLORS::BLACK: COLOR(1, 37)
        case CONSOLE_COLORS::WHITE: COLOR(0, 30)
        case CONSOLE_COLORS::DARK_GRAY: COLOR(0, 37)
#else
        case CONSOLE_COLORS::BLACK: COLOR(0, 30)
        case CONSOLE_COLORS::WHITE: COLOR(0, 37)
        case CONSOLE_COLORS::DARK_GRAY: COLOR(0, 30)
#endif
        case CONSOLE_COLORS::DARK_GREEN: COLOR(0, 32)
#if defined(WIN32)
        case CONSOLE_COLORS::PURPLE: COLOR(0, 95)
#else
        case CONSOLE_COLORS::PURPLE: COLOR(22, 35)
#endif
        case CONSOLE_COLORS::RED: COLOR(0, 31)
        case CONSOLE_COLORS::LIGHT_BLUE: COLOR(0, 94)
        case CONSOLE_COLORS::ORANGE: COLOR(0, 33)
            
        default:
            COLOR(0, 0)
    }
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "\033[%02d;%dm", prefix, final);
    return std::string(buffer) + (!enclose.empty() ? enclose + "\033[0m" : "");
#undef COLOR
}

template<CONSOLE_COLORS value, COLOR_TYPE type>
    requires (type == COLOR_TYPE::TAGS)
std::string console_color(const std::string& enclose = "") {
    std::string tag;
    switch (value) {
    case CONSOLE_COLORS::YELLOW: tag = "yellow"; break;
        case CONSOLE_COLORS::DARK_RED: tag = "darkred"; break;
        case CONSOLE_COLORS::GREEN: tag = "green"; break;
        case CONSOLE_COLORS::GRAY: tag = "gray"; break;
        case CONSOLE_COLORS::CYAN: tag = "cyan"; break;
        case CONSOLE_COLORS::DARK_CYAN: tag = "darkcyan"; break;
        case CONSOLE_COLORS::PINK: tag = "pink"; break;
        case CONSOLE_COLORS::BLUE: tag = "blue"; break;
        case CONSOLE_COLORS::BLACK: tag = "black"; break;
        case CONSOLE_COLORS::WHITE: tag = "white"; break;
        case CONSOLE_COLORS::DARK_GRAY: tag = "darkgray"; break;
        case CONSOLE_COLORS::DARK_GREEN: tag = "darkgreen"; break;
        case CONSOLE_COLORS::PURPLE: tag = "purple"; break;
        case CONSOLE_COLORS::RED: tag = "red"; break;
        case CONSOLE_COLORS::LIGHT_BLUE: tag = "lightblue"; break;
        case CONSOLE_COLORS::ORANGE: tag = "orange"; break;

        default:
            break;
    }

    if (enclose == " ")
        return " ";
    return "{" + tag + "}" + (enclose.empty() ? "" : (enclose + "{/"+tag+"}"));
}

template<CONSOLE_COLORS value, COLOR_TYPE type>
    requires (type == COLOR_TYPE::HTML)
std::string console_color(const std::string& enclose = "") {
    std::string tag;
    switch (value) {
    case CONSOLE_COLORS::YELLOW: tag = "yellow"; break;
    case CONSOLE_COLORS::DARK_RED: tag = "darkred"; break;
    case CONSOLE_COLORS::GREEN: tag = "green"; break;
    case CONSOLE_COLORS::GRAY: tag = "gray"; break;
    case CONSOLE_COLORS::CYAN: tag = "cyan"; break;
    case CONSOLE_COLORS::DARK_CYAN: tag = "darkcyan"; break;
    case CONSOLE_COLORS::PINK: tag = "pink"; break;
    case CONSOLE_COLORS::BLUE: tag = "blue"; break;
    case CONSOLE_COLORS::BLACK: tag = "black"; break;
    case CONSOLE_COLORS::WHITE: tag = "white"; break;
    case CONSOLE_COLORS::DARK_GRAY: tag = "darkgray"; break;
    case CONSOLE_COLORS::DARK_GREEN: tag = "darkgreen"; break;
    case CONSOLE_COLORS::PURPLE: tag = "purple"; break;
    case CONSOLE_COLORS::RED: tag = "red"; break;
    case CONSOLE_COLORS::LIGHT_BLUE: tag = "lightblue"; break;
    case CONSOLE_COLORS::ORANGE: tag = "orange"; break;

    default:
        break;
    }

    if (enclose == " ")
        return " ";
    return "<" + tag + ">" + (enclose.empty() ? "" : (utils::find_replace(enclose, { {"\n", "<br/>"},{"\t","&nbsp;&nbsp;&nbsp;&nbsp;"} }) + "</" + tag + ">"));
}


template<COLOR_TYPE colors = COLOR_TYPE::UNIX>
class ParseValue {
private:
    ParseValue() = delete;

public:
    template<typename T>
        requires _has_tostr_method<T>
    static std::string parse_value(const T& value) {
        auto str = value.toStr();
        return parse_value(str.c_str());
    }

    /*/template<typename T, typename K = remove_cvref_t<T>>
        requires std::convertible_to<T, std::string> && (!std::same_as<K, std::string>)
                && (!_is_dumb_pointer<T>)
    static std::string parse_value(const T& value) {
        return console_color<CONSOLE_COLORS::BLACK>() + std::string(value);
    }*/

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

        auto has_state = [&](State s) {
            return contains(states, s);
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
                    ss << console_color<CONSOLE_COLORS::DARK_GREEN, colors>(tmp);
                    break;
                case COMMENT:
                    ss << console_color<CONSOLE_COLORS::GREEN, colors>(tmp);
                    break;
                case ESCAPED_CHAR:
                    ss << console_color<CONSOLE_COLORS::DARK_RED, colors>(tmp);
                    break;
                case STRING:
                    //printf("STRING %s\n", tmp.c_str()); 
                    ss << console_color<CONSOLE_COLORS::RED, colors>() << tmp;
                    break;
                case NUMBER:
                    //printf("NUMBER %s\n", tmp.c_str()); 
                    ss << parse_value(Meta::fromStr<double>(tmp));
                    break;
                case CLASS:
                    //printf("CLASS %s\n", tmp.c_str()); 
                    ss << console_color<CONSOLE_COLORS::CYAN, colors>(tmp);
                    break;
                case NAMESPACE:
                    ss << console_color<CONSOLE_COLORS::DARK_CYAN, colors>(tmp);
                    break;
                case FUNCTION:
                    //printf("FUNCTION: %s\n", tmp.c_str());
                    if (!is_keyword(tmp))
                        ss << console_color<CONSOLE_COLORS::YELLOW, colors>(tmp);
                    else
                        ss << console_color<CONSOLE_COLORS::PURPLE, colors>(tmp);
                    break;
                case TAG:
                    //printf("TAG %s\n", tmp.c_str()); 
                    if (!is_keyword(tmp))
                        ss << console_color<CONSOLE_COLORS::CYAN, colors>(tmp);
                    else
                        ss << console_color<CONSOLE_COLORS::PURPLE, colors>(tmp);
                    break;
                case WHITESPACE: {
                    if (tmp == "::") {
                        //printf("TAG:: %s\n", tmp.c_str());
                        ss << console_color<CONSOLE_COLORS::DARK_GRAY, colors>(tmp);
                    }
                    else {
                        //printf("WHITESPACE (%d): %s\n", has_state(TAG), tmp.c_str());
                        ss << console_color<CONSOLE_COLORS::BLACK, colors>(tmp);
                    }
                    break;
                }

                default:
                    if (is_keyword(tmp)) {
                        ss << console_color<CONSOLE_COLORS::PURPLE, colors>(tmp);
                        //printf("word: %s\n", tmp.c_str());
                    }
                    else if (s == TAG) {
                        // printf("tag: %s\n", tmp.c_str());
                        if (tmp == ">")
                            ss << console_color<CONSOLE_COLORS::BLACK, colors>(tmp);
                    }
                    else if (is_state(CLASS)) {
                        //printf("GClass: '%s'\n", tmp.c_str());
                        ss << console_color<CONSOLE_COLORS::GRAY, colors>(tmp);
                    }
                    else {
                        //printf("Regular: %s\n", tmp.c_str());
                        ss << console_color<CONSOLE_COLORS::BLACK, colors>(tmp);
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

        auto pop_specific_state = [&](State s) {
            if (states.empty()) {
                //printf("States array empty\n");
                return;
            }

            auto current = states.back();
            for (int i = (int)states.size() - 1; i >= 0; --i) {
                if (states.at(i) == s) {
                    states.erase(states.begin() + i);

                    apply_state(states.back());
                    return;
                }
            }

            //printf("Cannot find state %d\n", s);
        };

        auto push_state = [&](State s) {
            apply_state(states.back());
            states.push_back(s);
        };

        static constexpr auto whitespace = " \t\n,^()[]{}<>/&%$§!?\\.:=-";

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
                if (s[i] == '\\' && i < N - 1 && alpha_numeric(s[i + 1]))
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
                push_state(WHITESPACE); tmp += s[i]; pop_state();
                push_state(TAG);
                continue;
            }
            else if (is_state(TAG) && s[i] == '>') {
                pop_state();
                push_state(WHITESPACE); tmp += s[i]; pop_state();
                continue;
            }
            else if (!is_state(NUMBER) && s[i] >= '0' && s[i] <= '9') {
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

    template<typename T, size_t N, typename K = remove_cvref_t<T>>
        requires std::convertible_to<T[N], std::string>
    static std::string parse_value(T(&s)[N]) {
        return parse_value((const char*)s);
    }

    template<typename T, typename K = remove_cvref_t<T>>
        requires _is_dumb_pointer<T> && std::same_as<T, const char*>
    static std::string parse_value(T s) {
        return pretty_text(s);
    }

    template<typename T, typename K = remove_cvref_t<T>>
        requires (!std::same_as<bool, K>) && (is_numeric<K> || std::unsigned_integral<K>)
    static std::string parse_value(T && value) {
        return console_color<CONSOLE_COLORS::GREEN, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = remove_cvref_t<T>>
        requires std::same_as<bool, K>
    static std::string parse_value(T value) {
        return console_color<CONSOLE_COLORS::PURPLE, colors>(Meta::toStr(value));
    }

    template<typename T, typename K = remove_cvref_t<T>>
        requires std::same_as<K, std::string>
    static std::string parse_value(const T& value) {
        return console_color<CONSOLE_COLORS::RED, colors>(Meta::toStr(value));
    }

    template<template <typename, typename> class T, typename A, typename B>
        requires is_pair<T<A, B>>::value
    static std::string parse_value(const T<A, B>& m) {
        return console_color<CONSOLE_COLORS::DARK_GRAY, colors>("[")
            + parse_value(m.first)
            + console_color<CONSOLE_COLORS::GRAY, colors>(",")
            + parse_value(m.second)
            + console_color<CONSOLE_COLORS::DARK_GRAY, colors>("]");
    }

    template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
        requires is_set<K>::value || is_container<K>::value
    static std::string parse_value(const T<Args...>&m) {
        std::string str = console_color<CONSOLE_COLORS::BLACK, colors>("[");
        auto it = m.begin(), end = m.end();
        for (;;) {
            str += parse_value(*it);

            if (++it != end)
                str += console_color<CONSOLE_COLORS::BLACK, colors>(",");
            else
                break;
        }
        return str + console_color<CONSOLE_COLORS::BLACK, colors>("]");
    }

    template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
        requires is_map<K>::value
    static std::string parse_value(const T<Args...>& m) {
        std::string str = console_color<CONSOLE_COLORS::BLACK, colors>("{");
        for (auto& [k, v] : m) {
            str += console_color<CONSOLE_COLORS::DARK_GRAY, colors>("[") 
                + parse_value(k) 
                + console_color<CONSOLE_COLORS::GRAY, colors>(":") 
                + parse_value(v) 
                + console_color<CONSOLE_COLORS::DARK_GRAY, colors>("]");
        }
        return str + console_color<CONSOLE_COLORS::BLACK, colors>("}");
    }

    template<typename T>
        requires _is_dumb_pointer<T> && (!std::same_as<const char*, T>)
    static std::string parse_value(T ptr) {
        std::string str;
        if constexpr (_has_class_name<std::remove_pointer_t<T>>)
            str += console_color<CONSOLE_COLORS::BLACK, colors>("(") 
            + console_color<CONSOLE_COLORS::CYAN, colors>((std::remove_pointer_t<T>::class_name()))
            + console_color<CONSOLE_COLORS::BLACK, colors>("*)");
        if (!ptr)
            return str + console_color<CONSOLE_COLORS::PURPLE, colors>("null");
        if constexpr (_has_tostr_method<std::remove_pointer_t<T>>)
            return str + parse_value(*ptr);

        std::stringstream stream;
        stream << "0x" << std::hex << (uint64_t)ptr;
        return str + console_color<CONSOLE_COLORS::LIGHT_BLUE, colors>(stream.str());
    }
};


template<COLOR_TYPE type, typename... Args>
std::string format(const Args& ... args) {
    std::string str = (... + (" " + ParseValue<type>::parse_value(args)));

    time_t rawtime;
    struct tm* timeinfo;
    char buffer[128];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return console_color<CONSOLE_COLORS::CYAN, type>("[" + std::string(buffer) + "]") + console_color<CONSOLE_COLORS::BLACK, type>(str);
}

static FILE* f{ nullptr };

template<typename... Args>
void print(const Args & ... args) {
    auto str = format<COLOR_TYPE::UNIX>(args...);
    printf("%s\n", str.c_str());

    if (!f) {
        f = fopen("site.html", "w");
        if (f) {
            static const char head[] =
                "<style>\n"
                "    body {\n"
                "        background:  black;\n"
                "        font-family: Cascadia Mono,Source Code Pro,monospace;\n"
                "    }\n"
                "    black { color: white; }\n"
                "    purple { color: purple; }\n"
                "    yellow { color: yellow; }\n"
                "    green { color:  green; }\n"
                "    red { color: red; }\n"
                "    darkgreen { color: darkgreen; }\n"
                "    lightblue { color: lightblue; }\n"
                "    cyan { color: cyan; }\n"
                "    darkred { color: darkred; }\n"
                "    darkcyan { color: darkcyan; }\n"
                "    gray {color: gray;}\n"
                "    darkgray {color:  darkgray;}\n"
                "</style>\n";

            fwrite((const void*)head, sizeof(char), sizeof(head) - 1, f);
        }
    }

    if (f) {
        str = format<COLOR_TYPE::HTML>(args...) + "<br/>\n";
        fwrite((void*)str.c_str(), sizeof(char), str.length()-1, f);
        fflush(f);
    }
}


int main(int argc, char**argv) {
    CommandLine cmd(argc, argv);
    cmd.cd_home();

    for (size_t i = 0; ; ++i) {
        print("Iteration", i);
        print("Program:\nint main() {\n\t// comment\n\tint value;\n\tscanf(\"%d\", &value);\n\tprintf(\"%d\\n\", value);\n\t/*\n\t * Multi-line comment\n\t */\n\tif(value == 42) {\n\t\tstd::string str = Meta::toStr(value);\n\t\tprintf(\"%s\\n\", str.c_str());\n\t}\n}");
        print("std::map<int,float>{\n\t\"key\":value\n}");

        std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));

        print("Python:\ndef func():\n\tprint(\"Hello world\")\n\nfunc()");

        std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));

        print("<html><head><style>.css-tag { bla; }</style></head><tag style='test'>tag content</tag><int></int></html>");

        std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));

        std::map<pv::bid, std::pair<std::string, float>> mappe;
        mappe[pv::bid::invalid] = { "Test", 0.5f };
        print("pv::bid:", pv::bid::invalid);
        print("<html><head><style>.css-tag { bla; }</style></head><tag>tag content</tag><int></int></html>");

        std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        print("Map:", mappe, FileSize{ uint64_t(1000 * 1000 * 2123) });
        print(std::set<pv::bid>{pv::bid::invalid});

        std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));

        pv::bid value;
        print(std::set<pv::bid*>{nullptr, & value});
        print(std::set<char*>{nullptr, (char*)0x123123, (char*)0x12222});
        print(std::vector<bool>{true, false, true, false});

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    exit(0);
    
    constexpr size_t count = 1000;
    {
        Timer timer;
        for (size_t i=0; i<count; ++i) {
            print("Test",i,"/",count);
        }
        
        auto seconds = timer.elapsed();
        print("This took", seconds,"seconds");
    }
    
    {
        Timer timer;
        for (size_t i=0; i<count; ++i) {
            Debug("Test %lu/%lu",i,count);
        }
        
        auto seconds = timer.elapsed();
        print("This took", seconds,"seconds");
    }
    
    print("Test",std::string("test"));
    //print("Test",5,6,7.0,std::vector<pv::bid>{pv::bid(1234), pv::bid::invalid},"MicroOwl starting...");
    print("A normal print, hello world.");

    constexpr auto owl_indexes = std::make_index_sequence<sizeof(owl)>{};
    constexpr auto owl_update = [] <std::size_t... Is> (std::index_sequence<Is...>) { (inc_offset<Is>(), ...); };
    constexpr auto owl_init = [] <std::size_t... Is> (std::index_sequence<Is...>) { (init_offset<Is>(), ...); };

    //! initialize color / position arrays
    owl_init(owl_indexes);

    //! initialize data struct for gui and timer (for physics)
    DrawStructure graph(1024,1024);
    Timer timer;
    Timer action_timer;
    Vec2 last_mouse_pos;
    
    //! open window and start looping
    bool terminate = false;
    IMGUIBase* ptr = nullptr;
    IMGUIBase base("BBC Micro owl", graph, [&]() -> bool {
        //! set dt and target position for the constexpr functions to exploit
        if (last_mouse_pos != graph.mouse_position()) {
            action_timer.reset();
            last_mouse_pos = graph.mouse_position();
            target = last_mouse_pos;
        }
        dt = timer.elapsed();

        if (action_timer.elapsed() > 3) {
            target = target + Size2(graph.width(), graph.height()).div(graph.scale())
                .mul(Vec2(rand() / float(RAND_MAX), rand() / float(RAND_MAX)) - 0.5).mul(0.55);
            if (target.x < radius * 18)
                target.x = radius * 18;
            if (target.y < radius * 18)
                target.y = radius * 18;
            if (target.x >= graph.width() / graph.scale().x - radius * 18)
                target.x = graph.width() / graph.scale().x - radius * 18;
            if (target.y >= graph.height() / graph.scale().y - radius * 18)
                target.y = graph.height() / graph.scale().y - radius * 18;
            action_timer.reset();
        }

        //! update positions based on the two gvars
        owl_update(owl_indexes);

        //! iterate the array in constexpr way (only the ones that are set)
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            ( action<Is>( graph ), ...);
        }(owl_indexes);
        
        
        graph.text("BBC MicroOwl", Vec2(10, 10), White, Font(1));
        e.update([](Entangled &e) {
            e.add<Rect>(Bounds(100, 100, 100, 25), White, Red);
        });
        graph.wrap_object(e);
        
        timer.reset();
        return !terminate;
        
    }, [&](const Event& e) {
        if (e.type == EventType::KEY && !e.key.pressed) {
            if (e.key.code == Codes::F && graph.is_key_pressed(Codes::LControl)) {
                ptr->toggle_fullscreen(graph);
            }
            else if (e.key.code == Codes::Escape)
                terminate = true;
        }
    });

    ptr = &base;
    base.loop();
    return 0;
}
