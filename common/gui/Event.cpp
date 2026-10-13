#include "Event.h"

namespace cmn::gui {

std::string Event::toStr() const {
    return "Event<" + Meta::toStr((uint32_t)type) + ">";
}

}
