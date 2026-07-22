#pragma once

#include <commons.pc.h>
#include <gui/dyn/Context.h>
#include <gui/dyn/State.h>

namespace cmn::gui::dyn {

template <typename... Args>
    //requires std::invocable<Fn, const Context&, State&>
auto bind_with_state(
    std::weak_ptr<CurrentObjectHandler> handler,
    const Context& context,
    auto&& fn)
{
    static_assert(std::invocable<decltype(fn), Args..., const Context&, State&>, "Function passed needs to accept const Context& and State& as well as the extra arguments.");
    auto _handler = handler.lock();
    if(not _handler)
        throw InvalidArgumentException("Invalid handler passed to bind_with_state.");
    auto snapshot = _handler->capture_scoped_variable_values();
    return [
        fn = std::move(fn),
        &context = context,
        snapshot = std::move(snapshot),
        handler = handler
    ](Args... args) {
        State state;
        state._current_object_handler = handler;
        auto _handler = handler.lock();
        if(not _handler)
            throw InvalidArgumentException("Handler has become invalid. Cannot execute action.");
        _handler->restore_scoped_variable_values(snapshot);
        return fn(std::forward<Args>(args)..., context, state);
    };
}

template <typename Fn>
    requires std::invocable<Fn, Event, const Context&, State&>
Drawable::callback_handle_t
bind_event_with_state(
    const Layout::Ptr& object,
    EventType type,
    std::weak_ptr<CurrentObjectHandler> handler,
    const Context& context,
    Fn&& fn,
    const Drawable::callback_handle_t::element_type* handle = nullptr)
{
    auto bound = bind_with_state<Event>(handler, context, [fn = std::move(fn)](Event e, const Context& context, State& state) {
        return fn(e, context, state);
    });
    auto object_ptr = std::weak_ptr<Drawable>(object.get_smart());

    return object->add_event_handler_replace(
        type,
        [bound = std::move(bound), handler, object_ptr](Event e) {
            if(auto current_handler = handler.lock(); current_handler) {
                if(auto current_object = object_ptr.lock(); current_object)
                    current_handler->select(current_object);
            }
            return bound(e);
        },
        handle
    );
}

}
