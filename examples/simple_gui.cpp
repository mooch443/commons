#include <gui/IMGUIBase.h>
#include <gui/DynamicGUI.h>
#include <misc/GlobalSettings.h>
#include <file/DataLocation.h>
#include <misc/CommandLine.h>

using namespace gui;
using namespace gui::dyn;

int main(int argc, char**argv) {
    // Initialize command line and working directory
    CommandLine::init(argc, argv);
    CommandLine::instance().cd_home();

    // Register custom data paths
    file::DataLocation::register_path("app", [](file::Path input) {
        return CommandLine::instance().wd() / input;
    });

    // Define global variables
    bool terminate = false;
    SETTING(app_name) = std::string("test application");
    SETTING(patharray) = file::PathArray("/Volumes/Public/work/*.mp4");
    SETTING(blob_size_ranges) = std::vector<float>{};
    SETTING(image_width) = int(1024);
    SETTING(region_model) = file::Path();
    
    // Create a list variable
    static std::vector<std::shared_ptr<VarBase_t>> list;
    std::vector<sprite::Map> _data;
    
    for(size_t i = 0; i<3; ++i) {
        sprite::Map tmp;
        tmp["name"] = std::string("object"+Meta::toStr(i));
        tmp["color"] = ColorWheel{static_cast<uint32_t>(i)}.next();
        tmp["pos"] = Vec2(100, 150+i*50);
        tmp["size"] = Size2(25, 25);
        _data.push_back(std::move(tmp));
        
        list.emplace_back(new Variable([i, &_data](std::string) -> sprite::Map& {
            return _data[i];
        }));
    }
    
    // Ability to add a module that makes a certain element draggable
    // (since this is not a native ability of the json)
    dyn::Modules::add(Modules::Module{
        ._name = "draggable",
        ._apply = [](size_t, State&, const Layout::Ptr& o) {
            o->set_draggable();
        }
    });
    
    // Initialize the GUI
    DrawStructure graph(1024, 1024);  // Window size
    IMGUIBase base("Dynamic GUI Example", graph, [&]() -> bool {
        // Initialize Dynamic GUI
        static dyn::DynamicGUI dynGUI{
            .base = &base,
            .graph = &graph,
            .path = file::DataLocation::parse("app", "test_gui.json"),  // JSON file location
            .context = {
                .actions = {
                    {
                        "QUIT", [&](auto) {
                            terminate = true;  // Quit action
                        }
                    }
                },
                .variables = {
                    {
                        "list_var",
                        std::unique_ptr<VarBase_t>(new Variable([](std::string) -> std::vector<std::shared_ptr<VarBase_t>>& {
                            return list;
                        }))
                    },
                    {
                        "global",
                        std::unique_ptr<VarBase_t>(new Variable([](std::string) -> sprite::Map& {
                            return GlobalSettings::map();
                        }))
                    }
                }
            }
        };

        // Update Dynamic GUI
        dynGUI.update(nullptr);

        return not terminate;  // Continue the event loop unless terminated
        
    }, [&](const Event& event){
        if(event.type == EventType::KEY
           && event.key.code == Keyboard::Escape)
        {
            terminate = true;
        }
    });

    base.loop();  // Start the GUI event loop
    return 0;
}
