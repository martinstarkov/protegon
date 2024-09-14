#include "event/event_handler.h"

namespace ptgn {

void EventHandler::Shutdown() {
	key	   = {};
	mouse  = {};
	window = {};
}

} // namespace ptgn