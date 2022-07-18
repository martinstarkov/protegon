#include "EventHandler.h"

#include <algorithm> // std::remove_if

namespace ptgn {

void EventHandler::Invoke(ecs::Entity& invoker) {
	/*auto caller_it{ instance.callers_.find(invoker) };
	assert(caller_it != std::end(instance.callers_) &&
		   "Could not invoke event on entity which has not registered such an event");
	for (const auto event_id : caller_it->second) {
		auto event_it{ instance.events_.find(event_id) };
		assert(event_it != std::end(instance.events_) &&
			   "Could not find valid event invoke function pointer");
		auto invoke_function{ event_it->second };
		assert(invoke_function != nullptr &&
			   "Could not create valid event invoke function pointer");
		invoke_function(invoker);
	}*/
}

void EventHandler::Remove(const ecs::Entity& invoker) {
	//GetInstance().callers_.erase(invoker);
}

void EventHandler::Update() {
	//auto& instance{ GetInstance() };
	//// Remove dead entities from event handler every cycle.
	//// This ensures event handler won't get bloated with dead entities.
	//for (auto it{ instance.callers_.begin() }; it != instance.callers_.end();) {
	//	if (!it->first.IsAlive()) {
	//		instance.callers_.erase(it++);
	//	} else {
	//		++it;
	//	}
	//}
}

EventHandler::EventId& EventHandler::EventTypeCount() {
	static EventId id{ 0 };
	return id;
}

} // namespace ptgn