#include "ListCache.h"
#include <processing/CPULabeling.h>
#include <processing/DLList.h>

namespace cmn::CPULabeling {

ListCache_t::ListCache_t()
    : obj(new DLList)
{

}

ListCache_t::~ListCache_t() {
    delete obj;
}

}
