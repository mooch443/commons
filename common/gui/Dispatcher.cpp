#include "Dispatcher.h"

namespace cmn::gui::attr {

void install_dispatcher_instance(Dispatcher* dispatcher) {
    Dispatcher::set_instance(dispatcher);
}

} // namespace cmn::gui::attr
