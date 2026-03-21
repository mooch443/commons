module;
#define IN_MODULE_INTERFACE 1

#include <misc/GlobalSettings.h>
#include <gui/GuiTypes.h>

export module commons;

export namespace cmn {
using ::cmn::bool_setting;
}

export namespace cmn::gui {
using ::cmn::gui::PrimitiveType;
}

#undef IN_MODULE_INTERFACE
