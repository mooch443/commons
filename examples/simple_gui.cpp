#include <gui/IMGUIBase.h>
#include <gui/DynamicGUI.h>
#include <misc/GlobalSettings.h>
#include <file/DataLocation.h>
#include <misc/CommandLine.h>
#include <gui/dyn/Action.h>

using namespace gui;
using namespace gui::dyn;

int main(int argc, char**argv) {
    // Initialize command line and working directory
    CommandLine::init(argc, argv);
    CommandLine::instance().cd_home();

    // Register custom data paths
    file::DataLocation::register_path("app", [](const sprite::Map&, file::Path input) {
        return CommandLine::instance().wd() / input;
    });

    // Define global variables
    bool terminate = false;
    SETTING(app_name) = std::string("test application");
    SETTING(patharray) = file::PathArray("/Volumes/Public/work/*.pt");
    SETTING(blob_size_ranges) = std::vector<float>{};
    SETTING(image_width) = int(1024);
    SETTING(region_model) = file::Path();
    
    // Create a list variable
    static std::vector<std::shared_ptr<VarBase_t>> list;
    std::vector<sprite::Map> _data;
    
    for(size_t i = 0; i<300; ++i) {
        sprite::Map tmp;
        tmp["name"] = std::string("object"+Meta::toStr(i));
        tmp["detail"] = std::string("detail");
        tmp["color"] = ColorWheel{static_cast<uint32_t>(i)}.next();
        tmp["pos"] = Vec2(100, 150+i*50);
        tmp["size"] = Size2(25, 25);
        _data.push_back(std::move(tmp));
        
        list.emplace_back(new Variable{
            [i, &_data](const VarProps&) -> sprite::Map& {
                return _data[i];
            }
        });
    }
    
    // Ability to add a module that makes a certain element draggable
    // (since this is not a native ability of the json)
    dyn::Modules::add(Modules::Module{
        ._name = "draggable",
        ._apply = [](size_t, State&, const Layout::Ptr& o) {
            o->set_draggable();
        }
    });

    // Open the Window
    IMGUIBase base("Dynamic GUI Example",
                   Size2{1024,1024},
                   [&](DrawStructure& graph) -> bool
    {
        // Initialize Dynamic GUI once, when the program starts
        // it contains all the context variables / actions for
        // the GUI itself, which is loaded form the JSON file.
        static dyn::DynamicGUI dynGUI{
            // JSON file location
            .path = file::DataLocation::parse("app", "test_gui.json"),
            .graph = &graph,
            .context = [&]() {
                /**
                 * In theory we could do all of this in one line, at least in most compilers.
                 * Visual Studio however currently (2022) has a bug that prevents this, 
                 * so we have to create an inline lambda function to split up the assignments of
                 * variables and actions. Otherwise "the compiler" will "run out of heap space".
                 */
                dyn::Context context;
                context.variables = {
                    // variables can be accessed in lots of ways,
                    // e.g. printed out or looped through within
                    // the json
                    VarFunc("list_var", [](const VarProps&) -> std::vector<std::shared_ptr<VarBase_t>>&{ return list; }),
                    VarFunc("global", [](const VarProps&) -> sprite::Map& { return GlobalSettings::map(); }),
                    VarFunc("isTrue", [](const VarProps&) { return true; }),
                    VarFunc("add", [](const VarProps&) { return true; }),
                    VarFunc("path", [](const VarProps&) { return file::Path("Herakles"); })
                };
                context.actions = {
                    // stuff that can be triggered by lists / buttons
                    // is an "Action". this quits the app:
                    ActionFunc("QUIT", [&](Action) { terminate = true; })
                };
                return context;
            }(),
            .base = &base
        };

        // Update Dynamic GUI
        dynGUI.update(nullptr);

        return not terminate;  // Continue the event loop unless terminated
        
    }, [&](auto&, const Event& event){
        if(event.type == EventType::KEY
           && event.key.code == Keyboard::Escape)
        {
            terminate = true;
        }
    });

    base.loop();  // Start the GUI event loop
    return 0;
}
