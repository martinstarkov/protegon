#pragma once

#include <cstdint>
#include <type_traits>

#include "core/util/hash.h"
#include "core/util/type_info.h"

namespace ptgn {

class EventDispatcher;
class EventHandler;
class Scene;
class Scripts;

namespace impl {

struct EventBase {
public:
	virtual ~EventBase() = default;

private:
	friend class EventDispatcher;

	bool event_handled_{ false };

	constexpr virtual std::size_t Type() = 0;
};

} // namespace impl

template <typename Derived>
struct Event : public impl::EventBase {
private:
	friend class EventDispatcher;

	static constexpr std::size_t event_id_{ Hash(type_name<Derived>()) };

	constexpr std::size_t Type() override {
		return Hash(type_name<Derived>());
	}
};

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
		if (e_.Type() != TEvent::event_id_) {
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

private:
	friend class EventHandler;
	friend class Scene;
	friend class Scripts;

	operator impl::EventBase&() const {
		return e_;
	}

	bool IsHandled() const {
		return e_.event_handled_;
	}

	impl::EventBase& e_;
};

} // namespace ptgn