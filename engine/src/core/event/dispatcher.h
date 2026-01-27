#pragma once

#include <type_traits>

#include "core/event/event.h"

namespace ptgn {

class EventDispatcher {
public:
	EventDispatcher(impl::EventBase& e) : e_(e) {}

	template <typename TEvent>
		requires std::is_base_of_v<impl::EventBase, std::remove_reference_t<TEvent>>
	EventDispatcher(TEvent&& evt) : e_{ evt } {}

	template <typename TEvent, typename TEventFn>
	void Dispatch(TEventFn&& fn) {
		if (e_.event_handled_) {
			return;
		}
		if (!IsType<TEvent>()) {
			return;
		}

		using TReturn = std::invoke_result_t<TEventFn, TEvent&>;

		if constexpr (std::is_same_v<TReturn, bool>) {
			if (fn(static_cast<TEvent&>(e_))) {
				e_.event_handled_ = true;
			}
		} else if constexpr (std::is_same_v<TReturn, void>) {
			fn(static_cast<TEvent&>(e_));
		}
	}

	operator impl::EventBase&() const {
		return e_;
	}

	bool IsHandled() const {
		return e_.event_handled_;
	}

	template <typename TEvent>
	constexpr bool IsType() const {
		static_assert(
			std::is_base_of_v<impl::EventBase, TEvent>, "TEvent must derive from EventBase"
		);
		return e_.Type() == TEvent::event_id_;
	}

private:
	impl::EventBase& e_;
};

} // namespace ptgn