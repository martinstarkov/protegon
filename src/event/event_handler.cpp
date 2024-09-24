#include "event/event_handler.h"

namespace ptgn::impl {

void EventHandler::UnsubscribeAll() {
	Reset();
}

void EventHandler::Reset() {
	key	   = {};
	mouse  = {};
	window = {};
}

void EventHandler::Shutdown() {
	Reset();
}

} // namespace ptgn::impl