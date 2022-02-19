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

using namespace DEBUG;

#if __APPLE__
#define XCODE_COLORS "OS_ACTIVITY_DT_MODE"
#endif

template<CONSOLE_COLORS value>
std::string console_color() {
#if /*!defined(NDEBUG) &&*/ defined(__APPLE__) || defined(__linux__)
    static std::once_flag flag;
    static bool dont_print = true;
    std::call_once(flag, [](){
#if __APPLE__
        char *xcode_colors = getenv(XCODE_COLORS);
        if (!xcode_colors || (strcmp(xcode_colors, "YES")!=0))
        {
#endif
            if(isatty(fileno(stdout)))
                dont_print = false;
#if __APPLE__
        }
#endif
    });
    
    if(dont_print)
        return "";
#endif
    
#define COLOR(PREFIX, FINAL) { prefix = PREFIX; final = FINAL; break; }
    int final = 0, prefix = 22;
    
    switch (value) {
        case YELLOW: COLOR(1, 33)
        case DARK_RED: COLOR(22, 31)
        case GREEN: COLOR(22, 32)
    #ifdef __APPLE__
        case GRAY: COLOR(1, 30)
    #else
        case GRAY: COLOR(22, 37)
    #endif
        case CYAN: COLOR(1,36)
        case DARK_CYAN: COLOR(22, 36)
        case PINK: COLOR(22,35)
        case BLUE: COLOR(22,34)
        case BLACK: COLOR(22,30)
        case DARK_GRAY: COLOR(1,30)
        case WHITE: COLOR(1, 37)
        case DARK_GREEN: COLOR(1, 32)
        case PURPLE: COLOR(22, 35)
        case RED: COLOR(1, 31)
        case LIGHT_BLUE: COLOR(1, 34)
            
        default:
            break;
    }
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "\033[%02d;%dm", prefix, final);
    return buffer;
#undef COLOR
}

template<typename T>
requires _is_dumb_pointer<T> && (!std::same_as<const char*, T>)
std::string parse_value(T ptr);

template<typename T>
    requires _has_tostr_method<T>
std::string parse_value(const T& value) {
    return console_color<CONSOLE_COLORS::DARK_GRAY>() + value.toStr();
}

template<typename T, typename K = remove_cvref_t<T>>
    requires std::convertible_to<T, std::string> && (!std::same_as<K, std::string>)
            && (!_is_dumb_pointer<T>)
std::string parse_value(const T& value) {
    return console_color<CONSOLE_COLORS::BLACK>() + std::string(value);
}


template<typename T, typename K = remove_cvref_t<T>>
    requires is_numeric<K> || std::unsigned_integral<K>
std::string parse_value(T&& value) {
    return console_color<CONSOLE_COLORS::GREEN>() + Meta::toStr(value);
}

template<typename T, typename K = remove_cvref_t<T>>
    requires std::same_as<K, std::string>
std::string parse_value(const T& value) {
    return console_color<CONSOLE_COLORS::RED>() + Meta::toStr(value);
}

template<template <typename, typename> class T, typename A, typename B>
    requires is_pair<T<A, B>>::value
std::string parse_value(const T<A, B>& m) {
    return console_color<CONSOLE_COLORS::DARK_GRAY>()
        + "["
        + parse_value(m.first)
        + console_color<CONSOLE_COLORS::GRAY>()
        + ","
        + parse_value(m.second)
        + console_color<CONSOLE_COLORS::DARK_GRAY>()
        + "]";
}
template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
    requires is_set<K>::value || is_container<K>::value
std::string parse_value(const T<Args...>& m) {
    std::string str = console_color<CONSOLE_COLORS::BLACK>() + "[";
    auto it = m.begin(), end = m.end();
    for (;;) {
        str += parse_value(*it);
        
        if(++it != end)
            str += console_color<CONSOLE_COLORS::BLACK>() + ",";
        else
            break;
    }
    return str + console_color<CONSOLE_COLORS::BLACK>() + "]";
}

template<template <typename...> class T, typename... Args, typename K = std::remove_cvref_t<T<Args...>>>
    requires is_map<K>::value
std::string parse_value(const T<Args...>& m) {
    std::string str = console_color<CONSOLE_COLORS::BLACK>() + "{";
    for (auto& [k, v] : m) {
        str += console_color<CONSOLE_COLORS::DARK_GRAY>() + "[" + parse_value(k) + console_color<CONSOLE_COLORS::GRAY>() + ":" + parse_value(v) + console_color<CONSOLE_COLORS::DARK_GRAY>() + "]";
    }
    return str+console_color<CONSOLE_COLORS::BLACK>() + "}";
}

template<typename T>
    requires _is_dumb_pointer<T> && (!std::same_as<const char*, T>)
std::string parse_value(T ptr) {
    std::string str;
    if constexpr(_has_class_name<std::remove_pointer_t<T>>)
        str += console_color<CONSOLE_COLORS::BLACK>() + "("+console_color<CONSOLE_COLORS::CYAN>()+(std::remove_pointer_t<T>::class_name())+console_color<CONSOLE_COLORS::BLACK>()+"*)";
    if(!ptr)
        return str + console_color<CONSOLE_COLORS::PURPLE>() + "null";
    if constexpr(_has_tostr_method<std::remove_pointer_t<T>>)
        return str + parse_value(*ptr);
    return str + "" + parse_value((uint64_t) ptr);
}

template<typename... Args>
void print(Args&& ... args) {
    std::string str = ((parse_value(std::forward<Args>(args)) + " ") + ...);
    
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[128];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    str = console_color<CONSOLE_COLORS::CYAN>() + "[" + buffer + "] " +console_color<CONSOLE_COLORS::BLACK>() + str;
    printf("%s\n", str.c_str());
}

int main(int argc, char**argv) {
    CommandLine cmd(argc, argv);
    cmd.cd_home();
    
    std::map<pv::bid, std::pair<std::string, float>> mappe;
    mappe[pv::bid::invalid] = {"Test", 0.5f};
    print("Map:",mappe, FileSize{uint64_t(1000 * 1000 * 2123)});
    print(std::set<pv::bid>{pv::bid::invalid});
    
    pv::bid value;
    print(std::set<pv::bid*>{nullptr, &value});
    print(std::set<char*>{nullptr, (char*)0x123123, (char*)0x12222});
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
