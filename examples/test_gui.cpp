#include <gui/IMGUIBase.h>
#include <misc/CommandLine.h>
#include <misc/PVBlob.h>
#include <file/Path.h>
#include <misc/SpriteMap.h>

#include <misc/format.h>
#include <gui/ControlsAttributes.h>
#include <gui/types/Button.h>

#include <gui/DynamicGUI.h>
#include <misc/GlobalSettings.h>

#include <file/DataLocation.h>

using namespace gui;
using namespace gui::dyn;

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
    Loc pos;
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

static constexpr Radius radius{5};
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
    graph.circle(offset[i].pos, radius + 3, LineClr{White.alpha(200)}, FillClr{offset[i].color});
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
    offset[i].pos += ((target + start - (float)radius * 18) - offset[i].pos) * dt
                         * ((start - 18 * (float)radius).length() / 100 + 1);
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

template<typename T, T... ints>
void print_sequence(std::integer_sequence<T, ints...> int_seq)
{
    std::cout << "The sequence of size " << int_seq.size() << ": ";
    ((std::cout << ints << ' '),...);
    ((printf("%s: %stext%s\n", FormatColor::names[ints], Formatter<FormatterType::UNIX, FormatColor::Class((FormatColor::data::values)ints)>::tag(), Formatter<FormatterType::UNIX, FormatColor::BLACK>::tag())), ...);
    
    std::cout << '\n';
}

using Objects = std::vector<Layout::Ptr>;
using Cell = Layout;
using Row = Layout;

auto cell = []<typename... T>(T... objects) {
    auto ptr = Layout::Make<Cell>(std::forward<T>(objects)...);
    ptr.template to<Cell>()->set(LineClr{Cyan});
    ptr.template to<Cell>()->set_name("Cell");
    return ptr;
};
auto row = []<typename... T>(T... objects) {
    Objects rowObjects;
    // Use a fold expression to expand the parameter pack
    ([&](auto&& arg) {
        if constexpr (std::is_same_v<decltype(arg), Objects>) {
            // If the argument is of type Objects, encapsulate it in a Cell
            rowObjects.push_back(cell(std::forward<decltype(arg)>(arg)));
        } else {
            // Otherwise, place the argument in an Objects and then in a Cell
            rowObjects.push_back(cell(Objects{std::forward<decltype(arg)>(arg)}));
        }
    }(objects), ...);
    auto ptr = Layout::Make<Row>(rowObjects);
    ptr.to<Row>()->set(LineClr{Yellow});
    ptr.to<Row>()->set_name("Row");
    return ptr;
};
auto horizontal = []<typename... T>(T... objects) {
    auto ptr = Layout::Make<HorizontalLayout>(Objects{std::forward<T>(objects)...});
    ptr.to<HorizontalLayout>()->set(LineClr{Purple});
    ptr.to<HorizontalLayout>()->set_name("Horizontal");
    return ptr;
};
auto circle = []<typename... T>(T... objects) {
    return Layout::Make<Rect>(std::forward<T>(objects)...);
};

// Overload for `+` to combine objects in a Row
Objects operator+(const Layout::Ptr& a, const Layout::Ptr& b) {
    return {a, b};
}

Objects operator+(const Layout::Ptr& a, const Objects& b) {
    Objects result = {a};
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

Objects operator+(const Objects& a, const Layout::Ptr& b) {
    Objects result(a);
    result.push_back(b);
    return result;
}

Objects operator+(const Objects& a, const Objects& b) {
    Objects result(a);
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

void print_layout_structure(Drawable* layout, int level = 0) {
    // Generate the indentation based on the level of the layout
    std::string indent(level * 4, ' ');

    std::cout << indent;

    if (dynamic_cast<GridLayout*>(layout)) {
        std::cout << "GridLayout\n";
    } else if (dynamic_cast<Row*>(layout)) {
        std::cout << "Row\n";
    } else if (dynamic_cast<Cell*>(layout)) {
        std::cout << "Cell\n";
    } else if (layout->type() == Type::CIRCLE) {
        std::cout << "Circle\n";
    }

    auto ptr = dynamic_cast<Layout*>(layout);
    if(ptr) {
        // Loop through the children and print their layout structure
        for (const auto& child : ptr->objects()) {
            if (child && child.is<Drawable>()) {
                print_layout_structure(child.to<Drawable>(), level + 1);
            }
        }
    }
}

int main(int argc, char**argv) {
    CommandLine::init(argc, argv);
    CommandLine::instance().cd_home();
    
    file::DataLocation::register_path("app", [](file::Path input) {
        return CommandLine::instance().wd() / input;
    });
    
    set_log_file("site.html");
    print("int main() {\n\tprintf();\n}\n");
    gui::init_errorlog();
    
    print(fmt::red("Hi in red."));
    
#if !defined(__EMSCRIPTEN__)
    try {
        throw SoftException("Soft exception ",argc);
    } catch(const SoftExceptionImpl& soft) {
        print("Got: ", soft);
    }
    try {
        throw CustomException(type<std::invalid_argument>, "Test",argc,argv);
    } catch(const std::invalid_argument& e) {
        print("Got: ",e);
    }
#endif
    
    print_sequence(std::make_index_sequence<FormatColor::data::num_elements>{});
    /*for (int j=0; j<11; ++j) {
     for(int k=0; k<11; ++k)
     for(int i=0; i<8; ++i)
     printf("[%d;%dm \033[%d;%dmtext\033[0;0m\n", j, j + k * 10 + i, j, j + k * 10 + i);
     }*/
    
    print("OpenGL1.32");
    print("str.c_str()");
    print("printf(\"%s\\n\", str.c_str());");
    print("dec<0>:", dec<0>( 0.423125123f ));
    print("Test \"\\\"string\\\"\".");
    print("File: ", file::Path("test"));
    print("dec<0>:", dec<0>( 0.523125123f ));
    print("dec<1>:", dec<1>( 0.523125123 ));
    print("dec<2>:", dec<2>( 0.523125123 ));
    print("dec<3>:", dec<3>( 0.523125123 ));
    print("dec<4>:", dec<4>( 0.523125123 ));
    print("dec<5>:", dec<5>( 0.523125123 ));
    print("dec<6>:", dec<6>( 0.523125123 ));
    FormatWarning("Something is wrong.");
    
    sprite::Map map;
    map["test"] = 25;
    map["path"] = file::Path("test");
    
    
    for (size_t i = 0; i<2; ++i) {
        print("Iteration ", i);
        print(std::vector<file::Path>{"/a/b/c","./d.exe"});
        print("Program:\nint main() {\n\t// comment\n\tint value;\n\tscanf(\"%d\", &value);\n\tprintf(\"%d\\n\", value);\n\t/*\n\t * Multi-line comment\n\t */\n\tif(value == 42) {\n\t\tstd::string str = Meta::toStr(value);\n\t\tprintf(\"%s\\n\", str.c_str());\n\t}\n}");
        print("std::map<std::string,float>{\n\t\"key\":value\n}");
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        
        print("Python:\ndef func():\n\tprint(\"Hello world\")\n\nfunc()");
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        
        print("<html><head><style>.css-tag { bla; }</style></head><tag style='test'>tag content</tag><int></int></html>");
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        
        std::map<pv::bid, std::pair<std::string, float>> mappe;
        mappe[pv::bid::invalid] = { "Test", 0.5f };
        print("pv::bid: ", pv::bid::invalid);
        print("<html><head><style>.css-tag { bla; }</style></head><tag>tag content</tag><int></int></html>");
        
        //std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        print("Map: ", mappe, " ", FileSize{uint64_t(1000 * 1000 * 2123)});
        print(std::set<pv::bid>{pv::bid::invalid});
        
        // std::this_thread::sleep_for(std::chrono::milliseconds(uint64_t(float(rand()) / float(RAND_MAX) * 1000)));
        
        pv::bid value;
        print(std::set<pv::bid*>{nullptr, & value});
        print(std::set<char*>{nullptr, (char*)0x123123, (char*)0x12222});
        print(std::vector<bool>{true, false, true, false});
        
        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    //exit(0);
    
    constexpr size_t count = 10;
    {
        Timer timer;
        for (size_t i=0; i<count; ++i) {
            print("Test ",i,"/",count);
        }
        
        auto seconds = timer.elapsed();
        print("This took ", seconds," seconds");
    }
    
    {
        Timer timer;
        for (size_t i=0; i<count; ++i) {
            printf("Test %lu/%lu\n",i, count);
        }
        
        auto seconds = timer.elapsed();
        print("This took ", seconds," seconds");
    }
    
    print("Test ",std::string("test"));
    //print("Test",5,6,7.0,std::vector<pv::bid>{pv::bid(1234), pv::bid::invalid},"MicroOwl starting...");
    print("A normal print, hello world.");
    
    constexpr auto owl_indexes = std::make_index_sequence<sizeof(owl)>{};
    constexpr auto owl_update = [] <std::size_t... Is> (std::index_sequence<Is...>) { (inc_offset<Is>(), ...); };
    constexpr auto owl_init = [] <std::size_t... Is> (std::index_sequence<Is...>) { (init_offset<Is>(), ...); };
    
    //! initialize color / position arrays
    owl_init(owl_indexes);
    
    //! initialize data struct for gui and timer (for physics)
    Timer timer;
    Timer action_timer;
    Vec2 last_mouse_pos;
    /*
     #if defined(__EMSCRIPTEN__)
     //file::Path image_path("/photomode_21012022_190427.png");
     file::Path image_path("./trex_screenshot_web.png");
     print("loading ", image_path);
     #else
     file::Path image_path("C:/Users/tristan/Pictures/Cyberpunk 2077/photomode_21012022_190427.png");
     #endif
     auto data = image_path.retrieve_data();
     if (data.empty())
     throw U_EXCEPTION("Image is empty.");
     auto mat = cv::imdecode(data, cv::ImreadModes::IMREAD_UNCHANGED);
     resize_image(mat, 0.25);
     
     ExternalImage image;
     image.set_source(std::make_unique<Image>(mat));
     image.set_scale(0.55);*/
    
    {
        
        Loc v(2, 3);
        Vec2 k(3, 2);
        
        auto r = v + k;
        auto r2 = k + v;
        print("Result ", r, " and ", r2, " ", Loc(2, 3) + Loc(3, 3));
    }
    
    // Current state:
    /*Button button({
     .text = "Test",
     .bounds = Bounds(100, 100, 100, 33),
     .text_clr = Cyan
     });*/
    
    using namespace attr;
    bool terminate = false;
    
    Button button("Text",
                  Loc(100, 100),
                  TextClr{Cyan});
    
    SETTING(app_name) = std::string("test application");
    SETTING(patharray) = file::PathArray("/Volumes/Public/work/*.mp4");
    SETTING(blob_size_ranges) = std::vector<float>{};
    SETTING(image_width) = int(1024);
    SETTING(region_model) = file::Path();
    
    GlobalSettings::map().set_do_print(true);
    
    dyn::Modules::add(Modules::Module{
        ._name = "draggable",
        ._apply = [](size_t, State&, const Layout::Ptr& o) {
            o->set_draggable();
        }
    });
    
    dyn::Modules::add(Modules::Module{
        ._name = "follow",
        ._apply = [](size_t index, State& state, const Layout::Ptr& o) {
            state.display_fns[index] = [o = o.get()](DrawStructure& g){
                o->set_pos(g.mouse_position() + Vec2(5));
            };
        }
    });
    
    static std::vector<std::shared_ptr<VarBase_t>> fishes;
    std::vector<sprite::Map> _data;
    
    for(size_t i = 0; i<3; ++i) {
        sprite::Map tmp;
        tmp["name"] = std::string("fish"+Meta::toStr(i));
        tmp["color"] = ColorWheel{static_cast<uint32_t>(i)}.next();
        tmp["pos"] = Vec2(100, 150+i*50);
        tmp["size"] = Size2(25, 25);
        tmp["blob_id"] = pv::bid(i * 1000);
        tmp["id"] = i;
        tmp["type"] = "ocelot";
        tmp["tracked"] = bool(i % 2 == 0);
        tmp["p"] = float(i) / float(3);
        tmp["px"] = 100;
        _data.push_back(std::move(tmp));
        
        fishes.emplace_back(new Variable([i, &_data](std::string) -> sprite::Map& {
            return _data[i];
        }));
    }
    
    ScrollableList<DetailItem> list(Bounds(200,200,300,400));
    list.set_items(std::vector<DetailItem>{
        { "<b>Title</b>", "detail" },
        { "Title2", "detail2" },
        { "Title3", "detail/to/other/path/very/long/text" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" },
        { "Title4", "d3t4il" }
    });
    list.set_font(Font(0.75, Align::Center));
    
    GridLayout layout;
    layout.set_name("GridLayout");
    auto circle0 = circle(Size{25,25}, FillClr{Red.alpha(150)});
    layout.set_children({
        row(circle(Size{25,25}, FillClr{Green.alpha(150)}),
            circle(Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}) + circle(Loc{25,25}, Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle0),
        
        row(circle(Size{25,25}, FillClr{Green.alpha(150)}),
            circle(Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle(Size{25,25}, FillClr{Red.alpha(150)})),
        
        row(circle(Size{25,25}, FillClr{Green.alpha(150)}),
            circle(Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle(Size{25,25}, FillClr{Red.alpha(150)})),
        
        row(circle(Size{25,25}, FillClr{Green.alpha(150)}) + circle(Loc{25,25}, Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle(Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle(Size{25,25}, FillClr{Red.alpha(150)})),
        
        row(circle(Size{25,25}, FillClr{Green.alpha(150)}),
            circle(Size{25,25}, FillClr{Transparent}, LineClr{Purple.alpha(150)}),
            circle(Size{25,25}, FillClr{Red.alpha(150)})),
    });
    layout.set_pos(Vec2(500,200));
    layout.set_clickable(true);
    
    //print_layout_structure(&layout);
    
    //! open window and start looping
    IMGUIBase* ptr = nullptr;
    IMGUIBase base("BBC Micro owl", {1024,1024},
        [&](DrawStructure& graph) -> bool
    {
        static dyn::DynamicGUI dynGUI{
            .path = file::DataLocation::parse("app", "test_gui.json"),
            .graph = &graph,
            .context = {
                .actions = {
                    {
                        "QUIT", [&](auto) {
                            terminate = true;
                        }
                    }
                },
                    .variables = {
                        {
                            "list_var",
                            std::unique_ptr<VarBase_t>(new Variable([](std::string) -> std::vector<std::shared_ptr<VarBase_t>>& {
                                return fishes;
                            }))
                        },
                        {
                            "isFalse",
                            std::unique_ptr<VarBase_t>(new Variable([](std::string){
                                return false;
                            }))
                        },
                        {
                            "isTrue",
                            std::unique_ptr<VarBase_t>(new Variable([](std::string){
                                return true;
                            }))
                        },
                        {
                            "global",
                            std::unique_ptr<VarBase_t>(new Variable([](std::string) -> sprite::Map& {
                                return GlobalSettings::map();
                            }))
                        }
                    }
            },
            .base = ptr
        };
        
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

        static double el = 0;
        el += dt;

        if (el >= 5) {
            //FormatWarning("A warning.");
            el = 0;
        }

        //! update positions based on the two gvars
        owl_update(owl_indexes);

        //! iterate the array in constexpr way (only the ones that are set)
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            ( action<Is>( graph ), ...);
        }(owl_indexes);
        
        graph.text(Str("BBC MicroOwl"), Loc(10, 10), TextClr(White.alpha(el / 5 * 205 + 50)), Font(1));
        graph.wrap_object(button);
        
        dynGUI.update(nullptr);
        
        auto size = circle0->size() + Size2(1);
        if(size.width > 50) {
            size = Size2(5);
        }
        circle0->set_size(size);
        
        graph.wrap_object(list);
        
        auto scale = graph.scale().reciprocal();
        ptr->window_dimensions().mul(scale * gui::interface_scale());
        graph.draw_log_messages();//Bounds(Vec2(0), dim));
        
        timer.reset();
        
        //graph.root().set_dirty();
        return !terminate;
        
    }, [&](DrawStructure& graph, const Event& e) {
        if (e.type == EventType::KEY && !e.key.pressed) {
            if (e.key.code == Codes::F && graph.is_key_pressed(Codes::LControl)) {
                ptr->toggle_fullscreen(graph);
            }
            else if (e.key.code == Codes::Escape)
                terminate = true;
            else if(e.key.code == Codes::Return) {
                print(layout.toString(nullptr));
            }
        }
    });

    ptr = &base;
    base.loop();
    return 0;
}
