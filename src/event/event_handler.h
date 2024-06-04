#pragma once

#include <functional>
#include <map>

#include "protegon/type_traits.h"
#include "protegon/events.h"

namespace ptgn {

template <typename T>
class EventDispatcher {
private:
	using SlotType = std::function<void(const Event<T>&)>;
public:
	// TODO: Figure out a better key to use than pointer cast.
	void Subscribe(void* ptr, const SlotType& func) {
		assert(ptr != nullptr && "Cannot add nullptr observer to EventDispatcher");
		observers_.emplace(ptr, func);
	};
	void Unsubscribe(void* ptr) {
		observers_.erase(ptr);
	}
	void Post(Event<T>&& event) {
		// This ensures that if a posted function modified observers, it does not invalidate the iterators.
		std::map<void*, SlotType> observers = observers_;
		for (auto&& [ptr, func] : observers) {
			if (ptr == nullptr || func == nullptr) continue;
			func(event);
		}
	};
	bool IsSubscribed(void* ptr) const {
		return observers_.find(ptr) != observers_.end();
	}
private:
	std::map<void*, SlotType> observers_;
};

class EventHandler {
public:
	EventDispatcher<MouseEvent> mouse_event;
};


} // namespace ptgn
