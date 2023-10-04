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
