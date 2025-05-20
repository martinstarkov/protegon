#include "events/event_handler.h"

#include "core/entity.h"

namespace ptgn::impl {

void EventHandler::UnsubscribeAll(const Entity& entity) {
	if (!entity) {
		return;
	}
	key.Unsubscribe(entity);
	mouse.Unsubscribe(entity);
	window.Unsubscribe(entity);
}

void EventHandler::Reset() {
	key.Reset();
	mouse.Reset();
	window.Reset();
}

void EventHandler::Shutdown() {
	Reset();
}

} // namespace ptgn::impl