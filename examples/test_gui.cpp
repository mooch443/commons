#include <gui/IMGUIBase.h>
#include <misc/CommandLine.h>

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

template<size_t i>
constexpr void action(DrawStructure& graph) {
    if constexpr (!is_pixel<i>())
        return;
    
    graph.circle(offset[i].pos, radius + 3, White.alpha(200), offset[i].color);
    graph.text("BBC MicroOwl", Vec2(10, 10), White, Font(1));
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

int main(int argc, char**argv) {
    CommandLine cmd(argc, argv);
    cmd.cd_home();

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
