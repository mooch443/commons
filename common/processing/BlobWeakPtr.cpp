#include "BlobWeakPtr.h"

#include <processing/PVBlob.h>

namespace pv {

std::string BlobWeakPtr::toStr() const {
    return ptr ? "(weak*)" + ptr->toStr() : "null";
}

}
