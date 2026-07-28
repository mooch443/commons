#pragma once
#include <commons.pc.h>
#include <misc/TaskQueue.h>

namespace cmn::gui {
class Base;
class DrawStructure;

using GUITaskQueue_t = TaskQueue<gui::Base*, gui::DrawStructure&>;
}
