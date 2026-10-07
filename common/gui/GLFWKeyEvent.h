#pragma once

#include <commons.pc.h>
#include <gui/Event.h>

namespace cmn::gui::detail {

// Translate GLFW input using the layout name supplied by the window callback.
COMMONS_EXPORT KeyEvent translate_glfw_key_event(
    int key, int scancode, int action, int mods, const char* name,
    std::unordered_map<int, Codes>& pressed_key_codes);

}
