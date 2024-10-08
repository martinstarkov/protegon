#pragma once

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "protegon/event.h"
#include "protegon/events.h"
#include "utility/debug.h"

namespace ptgn {

class EventHandler;

template <typename T>
class EventDispatcher {
	static_assert(std::is_enum_v<T>);

private:
	using Key = std::size_t;

	using GeneralEventCallback = std::function<void(T, const Event&)>;
	template <typename TEvent>
	using TEventCallback = std::function<void(const TEvent&)>;
	using EventCallback	 = TEventCallback<Event>;
	using EventCallbacks = std::unordered_map<T, EventCallback>;

public:
	// General event observation where type is passed to callback.
	template <typename S>
	void Subscribe(const S* ptr, GeneralEventCallback&& func) {
		PTGN_ASSERT(ptr != nullptr);
		auto key{ GetKey(ptr) };
		general_observers_[key] = func;
	}

	// Specific event observation.
	template <typename TEvent, typename S>
	void Subscribe(T type, const S* ptr, TEventCallback<TEvent>&& func) {
		PTGN_ASSERT(ptr != nullptr);
		auto key{ GetKey(ptr) };
		using TEventType = std::decay_t<TEvent>;
		static_assert(std::is_base_of_v<Event, TEventType>, "Events must inherit from Event class");
		auto it = observers_.find(key);
		std::pair<T, EventCallback> entry{ type, [&, func](const Event& event) {
											  const TEventType& e =
												  static_cast<const TEventType&>(event);
											  func(e);
										  } };
		if (it == observers_.end()) {
			observers_[key] = EventCallbacks{ entry };
		} else {
			it->second[entry.first] = entry.second;
		}
	}

	template <typename S>
	void Unsubscribe(const S* ptr) {
		PTGN_ASSERT(ptr != nullptr);
		auto key{ GetKey(ptr) };
		observers_.erase(key);
		general_observers_.erase(key);
	}

	template <typename TEvent>
	void Post(T type, TEvent&& event) const {
		// This ensures that if a posted function modified observers, it does
		// not invalidate the iterators.
		auto observers		   = observers_;
		auto general_observers = general_observers_;
		for (auto&& [key, callback] : general_observers) {
			callback(type, event);
		}
		for (auto&& [key, callbacks] : observers) {
			auto it = callbacks.find(type);
			if (it == std::end(callbacks)) {
				continue;
			}
			auto func = it->second;
			func(event);
		}
	};

	template <typename S>
	[[nodiscard]] bool IsSubscribed(const S* ptr) const {
		if (ptr == nullptr) {
			return false;
		}

		auto key{ GetKey(ptr) };

		return observers_.find(key) != observers_.end() ||
			   general_observers_.find(key) != general_observers_.end();
	}

private:
	template <typename S>
	[[nodiscard]] static Key GetKey(const S* ptr) {
		return std::hash<const void*>()(ptr);
	}

	// void* are used as general object keys, it does not own memory.

	std::unordered_map<Key, EventCallbacks> observers_;
	std::unordered_map<Key, GeneralEventCallback> general_observers_;
};

class EventHandler {
private:
	EventHandler()								 = default;
	~EventHandler()								 = default;
	EventHandler(const EventHandler&)			 = delete;
	EventHandler(EventHandler&&)				 = default;
	EventHandler& operator=(const EventHandler&) = delete;
	EventHandler& operator=(EventHandler&&)		 = default;

public:
	EventDispatcher<KeyEvent> key;
	EventDispatcher<MouseEvent> mouse;
	EventDispatcher<WindowEvent> window;

	// Unsubscribe from all built-in events (does not unsubscribe from custom user
	// EventDispatchers).
	template <typename S>
	void UnsubscribeAll(const S* ptr) {
		PTGN_ASSERT(ptr != nullptr);
		key.Unsubscribe(ptr);
		mouse.Unsubscribe(ptr);
		window.Unsubscribe(ptr);
	}

private:
	friend class Game;

	void Init() const {
		/* Possibly add things here in the future */
	}

	void Shutdown();
};

} // namespace ptgn
