#include "event/event_handler.h"

namespace ptgn::impl {

void EventHandler::UnsubscribeAll() {
	Reset();
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