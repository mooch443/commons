#include "DrawableCollection.h"
#include <gui/DrawStructure.h>

namespace cmn::gui {
    DrawableCollection::~DrawableCollection() {
        //DrawStructure::Lock_t *guard = NULL;
        //if(stage())
        //    guard = new GUI_LOCK(stage()->lock());
        set_parent(NULL);
        
        //if(guard)
        //    delete guard;
    }
}
