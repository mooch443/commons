#include "State.h"
#include <gui/LabeledField.h>

namespace cmn::gui::dyn {

State::State(const State& other)
    : patterns(other.patterns),
      display_fns(other.display_fns),
      ifs(other.ifs),
      _var_cache(other._var_cache),
      _customs(other._customs),
      _named_entities(other._named_entities),
      _current_object(other._current_object),
      _last_settings_box(other._last_settings_box),
      _settings_was_selected(other._settings_was_selected),
      _current_index(other._current_index)
{
    for(auto &[k, body] : other.loops) {
        loops[k] = {
            .variable = body.variable,
            .child = body.child,
            //.state = std::make_unique<State>(*body.state),
            .cache = body.cache
        };
    }
    for(auto &[k, body] : other.lists) {
        lists[k] = {
            .variable = body.variable,
            .item = body.item,
            .state = std::make_unique<State>(*body.state),
            .cache = body.cache,
            .on_select_actions = {}
        };
    }
    for (auto& [k, body] : other.manual_lists) {
        manual_lists[k] = {
            .items = body.items
        };
    }
}


}
