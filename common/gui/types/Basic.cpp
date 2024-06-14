#include "Basic.h"

namespace cmn::gui {
    std::ostream & Vertex::operator <<(std::ostream &os) {
        throw U_EXCEPTION("Nope.");
        return os;
    }

    std::string Vertex::toStr() const {
        return "V<"+Meta::toStr(position()) + ">";
    }
}
