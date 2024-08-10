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
#include <gui/dyn/Action.h>
#include <video/FFmpegVideoCapture.h>
#include <video/VideoSource.h>
#include <file/PathArray.h>
#include <file/Path.h>

using namespace cmn;
using namespace gui;
using namespace gui::dyn;

int main(int argc, char**argv) {
    // Initialize command line and working directory
    CommandLine::init(argc, argv);
    CommandLine::instance().cd_home();

    // Register custom data paths
    file::DataLocation::register_path("app", [](const sprite::Map&, file::Path input) {
        if(input.is_absolute())
            return input;
        return CommandLine::instance().wd() / input;
    });

    // Define global variables
    bool terminate = false;
    
    SETTING(app_name) = std::string("test application");
    SETTING(patharray) = file::PathArray("/Volumes/Public/work/*.pt");
    SETTING(track_size_filter) = std::vector<float>{};
    SETTING(image_width) = int(1024);
    SETTING(region_model) = file::Path();
    SETTING(gui_frame) = Frame_t(0);
    SETTING(gui_run) = false;
    SETTING(terminate) = false;
    SETTING(mat) = cv::Mat();
    
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
        tmp["visible"] = i % 2 == 0;
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
    
    ExternalImage display;
    display.set_source(Image::Make());
    GUITaskQueue_t queue;
    dyn::DynamicGUI dynGUI;

    // Open the Window
    IMGUIBase base("Dynamic GUI Example",
                   Size2{1024,1024},
                   [&](DrawStructure& graph) -> bool
    {
        if(not dynGUI) {
            // Initialize Dynamic GUI once, when the program starts
            // it contains all the context variables / actions for
            // the GUI itself, which is loaded form the JSON file.
            dynGUI = dyn::DynamicGUI{
                // JSON file location
                .gui = &queue,
                .path = file::DataLocation::parse("app", "test_gui.json"),
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
                        VarFunc("path", [](const VarProps&) { return file::Path("Herakles"); }),
                        VarFunc("window_size", [](const VarProps&){
                            return Size2(1024,1024);
                        }),
                        VarFunc("video_length", [](const VarProps&) -> Frame_t {
                            return 5000_f;
                        }),
                        VarFunc("mouse", [&](const VarProps&) -> Vec2 {
                            return graph.mouse_position();
                        })
                    };
                    context.actions = {
                        // stuff that can be triggered by lists / buttons
                        // is an "Action". this quits the app:
                        ActionFunc("QUIT", [&](Action) { terminate = true; }),
                        ActionFunc("set", [&](Action a) {
                            if(a.parameters.size() < 2)
                                throw InvalidArgumentException("Not enough arguments for set:name: ", a);
                            if(not GlobalSettings::has(a.parameters.at(0)))
                                throw InvalidArgumentException("Cannot find given parameter: ", a);
                            GlobalSettings::map()[a.parameters.at(0)].get().set_value_from_string(a.parameters.at(1));
                        })
                    };
                    return context;
                }(),
                .base = &base
            };
        }
        graph.wrap_object(display);

        // Update Dynamic GUI
        dynGUI.update(graph, nullptr);
        queue.processTasks(&base, graph);

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

```

For the JSON configuration (\`test_gui.json\`) that accompanies this example, you might have something like:

```json
{ "objects": [
    { "type":"vlayout", "pos":[20,10], "pad":[5,5,5,5],
      "children": [
      { "type": "hlayout", "pad":[0,0,10,5], "children": [
          { "type": "button", "text": "Quit", "action": "QUIT"},
          { "type": "collection", "children": [
            {"type": "text", "text": "Hello World!"}
          ]}
      ]},
      { "type": "textfield",
        "size":[500,40],
        "color":[255,0,255,255],
        "action":"QUIT"
      },
      {"type":"stext",
      "text":"{mouse.x} {window_size.w} {video_length} -- {*:{video_length}:{/:{mouse.x}:{window_size.w}}}"
    },
      {"type": "vlayout", "pad":[5,5,5,5], "children": [
          {"type":"settings","var": "app_name","fill": [50,50,50,125],"size":[300,40]},
          { "type":"settings","var": "patharray", 
            "fill": [50,50,50,125], 
            "size":[800,40],
            "preview":{
              "max_size":[800,100],
              "font": { "size": 0.5 }
            }
          },
          {"type":"settings","var":"region_model","fill":[50,50,50,125],"size":[300,40]},
          {"type":"settings","var":"track_size_filter","fill":[50,50,50,125],"size":[300,40]}
      ]},
      { "type": "gridlayout", 
        "pos": [550, 600], 
        "horizontal_clr": [50,50,150,100], 
        "vertical_clr": [50,150,150,100],
        "line":[50,50,50,200],
        "pad":[5,5,5,5],
        "clickable": true, 
        "children": [
        [ 
          [{"type": "text", "text": "Entry 1"}], 
          [{"type": "stext", 
            "text": "<sym>▶⏸⏹📁❤⚖️⚠✓✂☁</sym> <key><c>Привет</c></key> {add} {path}", 
            "max_size":[500,50], 
            "font": { "size": 0.5 }
          }] 
        ],
        [ 
          [{"type": "text", "text": "region={global.region_model}"}], 
          [{"type": "text", "text": "app={global.app_name}"}]
        ],
        [
          [{"type":"settings","var":"gui_frame","fill":[50,50,50,125],"size":[300,40]}],
          [{"type":"settings","var":"app_name","fill":[50,50,50,125],"size":[300,40]}],
          [{"type":"button","action":"set:gui_run:true","fill":[50,50,50,125],"size":[300,40]}]
        ]
      ]},
      { "type":"rect","pos":[0,0],"size": [50,50],"fill":[255,0,0,125]},
      { "type":"circle","pos":[0,0],"radius":10,"fill":[125,125,0,125]},
      { "type":"condition", "var": "isTrue",
        "then":{"type":"stext","text":"The value is <key>True</key>."}, 
        "else":{"type":"stext","text":"The value is <key>False</key>."}
      },
      { "type":"list",
        "var":"list_var",
        "size":[300,300],
        "line":[255,0,0,125],
        "foldable":true,
        "folded":true,
        "text":"test",
        "font":{ "size":0.75, "align":"center" },
        "template": {
            "text":"{i.name}",
            "detail":"{i.detail}",
            "action":"open_recent"
        }
      }
    ]},
    { "type":"vlayout",
      "modules":["draggable"],
      "line":[125,0,0,200],
      "fill":[125,0,0,100],
      "pos":[500,10],
      "children": [
      { "type": "each", 
        "var": "list_var", 
        "do": {
          "type":"condition",
          "var":"{||:{i.visible}:{equal:{global.gui_frame}:0}}",
          "then": {
            "type": "stext",
            "text": "{if:{equal:{global.gui_frame}:0}:'gui_frame is 0':'not zero {global.gui_frame}'} {i.name} {i.color}", 
            "color":"{i.color}",
            "align":"center"
          },
          "else": {
            "type": "condition",
            "var":"{equal:{global.gui_frame}:2}",
            "then": {
              "type": "rect",
              "size": [100,100],
              "fill": [255,255,255,255],
              "line": "{i.color}"
            },
            "else": {
              "type": "stext",
              "text": "{i.name}",
              "color":"{i.color}"
            }
          }
        }
      }
    ]}
]}

```

## Contributions

Feel free to contribute to the project by opening Pull Requests or Issues. Please follow the coding guidelines and keep your code as clean as possible. Thank you!

---

For more details, please refer to the documentation and examples provided.

# Contributors, Issues, etc.

If you want to contribute, please submit a [pull request](https://github.com/mooch443/commons/pulls) on GitHub and I will be happy to credit you here, for any substantial contributions!

# License

Released under the GPLv3 License (see [LICENSE](https://github.com/mooch443/commons/blob/master/LICENSE)).
