#pragma once

#include <type_traits>

#include "core/event/event.h"

namespace ptgn {

/// @brief Dispatcher for routing events to matching handlers.
///
/// If a handler returns true, the event is marked as handled, otherwise the event keeps
/// propagating.
///
/// Example:
/// @code
/// void OnEvent(EventDispatcher d) {
///     d.Dispatch<WindowResized>([](WindowResized& e) {
///         DoStuff(e.size);
///     });
/// }
/// @endcode
class EventDispatcher {
public:
	EventDispatcher(impl::EventBase& event) : event_{ event } {}

	template <EventType T>
	EventDispatcher(T& event) : event_{ event } {}

	/// @brief Dispatches the event to the given callable if types match.
	///
	/// The callable must accept `T&`.
	/// - If it returns `bool` and returns `true`, the event is marked handled.
	/// - If it returns `void`, the event keeps propagating.
	template <EventType T, typename TEventFn>
	void Dispatch(TEventFn&& fn) {
		if (event_.event_handled_) {
			return;
		}
		if (!IsType<T>()) {
			return;
		}

		using TReturn = std::invoke_result_t<TEventFn, T&>;

		if constexpr (std::is_same_v<TReturn, bool>) {
			if (fn(static_cast<T&>(event_))) {
				event_.event_handled_ = true;
			}
		} else if constexpr (std::is_same_v<TReturn, void>) {
			fn(static_cast<T&>(event_));
		}
	}

	/// @brief Dispatches to a member function returning `void`.
	template <EventType T, typename TObject>
	void Dispatch(void (TObject::*memfn)(T&), TObject* obj) {
		Dispatch<T>([obj, memfn](T& e) { (obj->*memfn)(e); });
	}

	/// @brief Dispatches to a member function returning `bool`.
	template <EventType T, typename TObject>
	void Dispatch(bool (TObject::*memfn)(T&), TObject* obj) {
		Dispatch<T>([obj, memfn](T& e) { return (obj->*memfn)(e); });
	}

	operator impl::EventBase&() const {
		return event_;
	}

	/// @return True if the event has been handled.
	bool IsHandled() const {
		return event_.event_handled_;
	}

	/// @return True if the stored event is of type `T`.
	template <EventType T>
	constexpr bool IsType() const {
		return event_.Type() == T::event_id_;
	}

	constexpr bool IsType(std::size_t event_id) const {
		return event_.Type() == event_id;
	}

private:
	impl::EventBase& event_;
};

} // namespace ptgn