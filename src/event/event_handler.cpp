#include "event_handler.h"

namespace ptgn {

void EventHandler::Init() {}

void EventHandler::Shutdown() {
	key	   = {};
	mouse  = {};
	window = {};
}

} // namespace ptgn