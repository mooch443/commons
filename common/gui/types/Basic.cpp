#include "Basic.h"

namespace cmn::gui {
    std::ostream & Vertex::operator <<(std::ostream &os) {
        throw U_EXCEPTION("Nope.");
        return os;
    }

    std::string Vertex::toStr() const {
        return "V<"+Meta::toStr(position()) + ">";
    }

    std::string Font::toStr() const {
        return "Font<"+Meta::toStr(size)+" "+Meta::toStr(style)+" "+Meta::toStr((int)align)+">";
    }
}
