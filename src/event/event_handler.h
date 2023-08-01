#pragma once

#include <functional> // std::function
#include <map>        // std::map

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
		observers_.emplace(ptr, func);
	};
	void Unsubscribe(void* ptr) {
		observers_.erase(ptr);
	}
	void Post(Event<T>&& event) {
		for (auto&& [_, func] : observers_)
			func(event);
	};
private:
	std::map<void*, SlotType> observers_;
};

class EventHandler {
public:
	EventDispatcher<MouseEvent> mouse_event;
private:
};


} // namespace ptgn
