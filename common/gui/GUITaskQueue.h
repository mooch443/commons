#pragma once
#include <commons.pc.h>
#include <misc/TaskQueue.h>

namespace gui {
class IMGUIBase;
class DrawStructure;

using GUITaskQueue_t = TaskQueue<IMGUIBase*, DrawStructure&>;
}