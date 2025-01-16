#pragma once

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "event/event.h"
#include "event/events.h"
#include "utility/debug.h"

namespace ptgn {

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
	void Subscribe(const S* ptr, const GeneralEventCallback& func) {
		PTGN_ASSERT(ptr != nullptr);
		auto key{ GetKey(ptr) };
		general_observers_[key] = func;
	}

	// Specific event observation.
	template <typename TEvent, typename S>
	void Subscribe(T type, const S* ptr, const TEventCallback<TEvent>& func) {
		PTGN_ASSERT(ptr != nullptr);
		auto key{ GetKey(ptr) };
		using TEventType = std::decay_t<TEvent>;
		static_assert(std::is_base_of_v<Event, TEventType>, "Events must inherit from Event class");
		auto it = observers_.find(key);
		std::pair<T, EventCallback> entry{ type, std::function([func](const Event& event) {
											   const auto& e =
												   static_cast<const TEventType&>(event);
											   func(e);
										   }) };
		if (it == observers_.end()) {
			observers_[key] = EventCallbacks{ entry };
		} else {
			it->second[entry.first] = entry.second;
		}
	}

	template <typename S>
	void Unsubscribe(const S* ptr) {
		if (ptr == nullptr) {
			return;
		}
		Unsubscribe(GetKey(ptr));
	}

	template <typename TEvent>
	void Post(T type, const TEvent& event) const {
		// This ensures that if a posted function modified observers, it does
		// not invalidate the iterators.
		auto general_observers = general_observers_;
		for (const auto& [key, callback] : general_observers) {
			if (general_observers_.empty()) {
				break;
			}
			if (general_observers_.find(key) == general_observers_.end()) {
				continue;
			}
			if (callback != nullptr) {
				std::invoke(callback, type, event);
			}
		}
		auto observers = observers_;
		for (const auto& [key, callbacks] : observers) {
			auto it = callbacks.find(type);
			if (it == std::end(callbacks)) {
				continue;
			}
			if (observers_.empty()) {
				break;
			}
			if (observers_.find(key) == observers_.end()) {
				continue;
			}
			if (it->second != nullptr) {
				std::invoke(it->second, event);
			}
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

	void Reset() {
		// Cannot use map.clear() because the destructors of observer objects may themselves call
		// unsubscribe which causes a deallocation exception to be thrown.
		while (!observers_.empty()) {
			auto it = observers_.begin();
			// Unsubscribe can invalidate the iterator if the callback function stores a shared ptr
			// the destructor of which calls Unsubscribe().
			Unsubscribe(it->first);
		}
		while (!general_observers_.empty()) {
			auto it = general_observers_.begin();
			Unsubscribe(it->first);
		}
		PTGN_ASSERT(observers_.empty());
		PTGN_ASSERT(general_observers_.empty());
	}

private:
	template <typename S>
	[[nodiscard]] static Key GetKey(const S* ptr) {
		return std::hash<const void*>()(ptr);
	}

	void Unsubscribe(std::size_t key) {
		if (auto it{ observers_.find(key) }; it != observers_.end()) {
			observers_.erase(it);
		}
		if (auto it{ general_observers_.find(key) }; it != general_observers_.end()) {
			general_observers_.erase(it);
		}
	}

	// void* are used as general object keys, it does not own memory.

	std::unordered_map<Key, EventCallbacks> observers_;
	std::unordered_map<Key, GeneralEventCallback> general_observers_;
};

namespace impl {

class EventHandler {
public:
	EventHandler()								 = default;
	~EventHandler()								 = default;
	EventHandler(const EventHandler&)			 = delete;
	EventHandler(EventHandler&&)				 = default;
	EventHandler& operator=(const EventHandler&) = delete;
	EventHandler& operator=(EventHandler&&)		 = default;

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

	void Reset();

	void Init() const {
		/* Possibly add things here in the future */
	}

	void Shutdown();
};

} // namespace impl

} // namespace ptgn
