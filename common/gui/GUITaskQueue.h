#pragma once
#include <commons.pc.h>
#include <misc/TaskQueue.h>

namespace cmn::gui {
class IMGUIBase;
class DrawStructure;

using GUITaskQueue_t = TaskQueue<gui::IMGUIBase*, gui::DrawStructure&>;
}

