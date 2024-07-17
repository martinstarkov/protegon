#pragma once

#include <functional>
#include <unordered_map>
#include <type_traits>
#include <cstdint>

#include "protegon/events.h"
#include "utility/type_traits.h"

namespace ptgn {


template <typename T>
class EventDispatcher {
	static_assert(std::is_enum_v<T>);

private:
	using GeneralEventCallback = std::function<void(T, const Event&)>;
	template <typename TEvent>
	using TEventCallback = std::function<void(const TEvent&)>;
	using EventCallback	 = TEventCallback<Event>;
	using EventCallbacks = std::unordered_map<T, EventCallback>;

public:
	// General event observation where type is passed to callback.
	void Subscribe(void* ptr, GeneralEventCallback&& func) {
		general_observers_[ptr] = func;
	}

	// Specific event observation.
	template <typename TEvent>
	void Subscribe(T type, void* ptr, TEventCallback<TEvent>&& func) {
		using TEventType = std::decay_t<TEvent>;
		static_assert(std::is_base_of_v<Event, TEventType>, "Events must inherit from Event class");
		auto it = observers_.find(ptr);
		std::pair<T, EventCallback> entry{ type, [&, func](const Event& event) {
											  const TEventType& e =
												  static_cast<const TEventType&>(event);
											  func(e);
										  } };
		if (it == observers_.end()) {
			observers_[ptr] = EventCallbacks{ entry };
		} else {
			it->second[entry.first] = entry.second;
		}
	}

	void Unsubscribe(void* ptr) {
		observers_.erase(ptr);
		general_observers_.erase(ptr);
	}

	template <typename TEvent>
	void Post(T type, TEvent&& event) {
		// This ensures that if a posted function modified observers, it does
		// not invalidate the iterators.
		auto observers		   = observers_;
		auto general_observers = general_observers_;
		for (auto&& [ptr, callback] : general_observers) {
			if (ptr == nullptr) {
				continue;
			}
			callback(type, event);
		}
		for (auto&& [ptr, callbacks] : observers) {
			if (ptr == nullptr) {
				continue;
			}
			auto it = callbacks.find(type);
			if (it == callbacks.end()) {
				continue;
			}
			auto func = it->second;
			func(event);
		}
	};

	[[nodiscard]] bool IsSubscribed(void* ptr) const {
		return observers_.find(ptr) != observers_.end() ||
			   general_observers_.find(ptr) != general_observers_.end();
	}

private:
	// void* are used as general object keys, it does not own memory.

	std::unordered_map<void*, EventCallbacks> observers_;
	std::unordered_map<void*, GeneralEventCallback> general_observers_;
};

class EventHandler {
public:
	EventDispatcher<MouseEvent> mouse;
	EventDispatcher<WindowEvent> window;
};

} // namespace ptgn
