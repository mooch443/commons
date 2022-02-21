#include "Basic.h"

namespace gui {
    std::ostream & Vertex::operator <<(std::ostream &os) {
        throw U_EXCEPTION("Nope.");
        return os;
    }
}
