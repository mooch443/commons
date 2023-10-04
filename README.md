<!--<p align="center"><img src="https://github.com/mooch443/trex/blob/master/images/Icon1024.png" width="160px"></p>-->

[![Windows](https://github.com/mooch443/commons/actions/workflows/cmake-windows.yml/badge.svg?branch=main)](https://github.com/mooch443/commons/actions/workflows/cmake-windows.yml) [![MacOS](https://github.com/mooch443/commons/actions/workflows/cmake-macos.yml/badge.svg?branch=main)](https://github.com/mooch443/commons/actions/workflows/cmake-macos.yml/badge.svg?branch=main) [![Linux](https://github.com/mooch443/commons/actions/workflows/cmake-linux.yml/badge.svg?branch=main)](https://github.com/mooch443/commons/actions/workflows/cmake-linux.yml)

# Commons Library

## Overview

The Commons Library is a GPLv3-licensed C++ library that aims to provide a collection of utility functions and classes for various use-cases. It includes modules for file handling, GUI, HTTP, miscellaneous utilities, data processing, tests, and video.

## Getting Started

### Prerequisites

Make sure you have CMake installed on your system to build the project.

### Clone the Repository

To clone the Commons Library repository, run the following command:

```bash
git clone https://github.com/YourGitHubAccount/commons.git
```

### Build with CMake

Navigate to the root directory of the cloned repository and run the following commands:

```bash
mkdir build
cd build
cmake ..
make
```

## Example: Dynamic GUI

Here is a simple example that demonstrates the use of dynamic GUI functionality in the Commons Library:

```cpp
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

```

For the JSON configuration (\`test_gui.json\`) that accompanies this example, you might have something like:

```json
{
    "objects": [{
        "type": "vlayout", "pos": [20, 10],
        "children": [
            {"type": "hlayout", "children": [
                {"type": "button", "text": "Quit", "action": "QUIT"},
                {"type": "collection", "pos": [300, 100], "children": [
                    {"type": "text", "text": "Hello World!"}
                ]}
            ]},
            {"type": "vlayout", "children": [
                {"type": "settings", "var": "app_name", "fill": [50, 50, 50, 125]},
                {"type": "settings", "var": "patharray", "fill": [50, 50, 50, 125]},
                {"type": "settings", "var": "region_model", "fill": [50, 50, 50, 125]},
                {"type": "settings", "var": "blob_size_ranges", "fill": [50, 50, 50, 125]}
            ]},
            {"type": "gridlayout", "pos": [550, 600], "horizontal_clr": [50, 50, 150, 100], "vertical_clr": [50, 150, 150, 100], "clickable": true, "children": [
                [ 
                  [{"type": "text", "text": "Entry 1"}], 
                  [{"type": "text", "text": "app={global:patharray}"}] 
                ],
                [ 
                  [{"type": "text", "text": "app={global:region_model}"}], 
                  [{"type": "text", "text": "app={global:app_name}"}]
                ]
            ]},
            {"type": "rect", "pos": [0, 0], "size": [50, 50], "fill": [255, 0, 0, 125]},
            {"type": "circle", "pos": [0, 0], "radius": 10, "fill": [125, 125, 0, 125]},
            {"type": "each", "var": "list_var", "do": {"type": "text", "text": "{i:item}"}},
            {"type": "condition", "var": "isTrue", "then": {"type": "text", "text": "True"}, "else": {"type": "text", "text": "False"}}
        ]
    }]
}

```

## Contributions

Feel free to contribute to the project by opening Pull Requests or Issues. Please follow the coding guidelines and keep your code as clean as possible. Thank you!

---

For more details, please refer to the documentation and examples provided.

# Contributors, Issues, etc.

If you want to contribute, please submit a [pull request](https://github.com/mooch443/commons/pulls) on GitHub and I will be happy to credit you here, for any substantial contributions!

# License

Released under the GPLv3 License (see [LICENSE](https://github.com/mooch443/commons/blob/master/LICENSE)).
