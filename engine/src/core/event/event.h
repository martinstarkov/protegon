#pragma once

#include <cstdint>
#include <type_traits>

#include "core/util/hash.h"
#include "core/util/type_info.h"

namespace ptgn {

class EventDispatcher;

namespace impl {

struct EventBase {
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

	constexpr virtual std::size_t Type() override {
		return Hash(type_name<Derived>());
	}
};

class EventDispatcher {
public:
	EventDispatcher(impl::EventBase& e) : e_(e) {}

	template <typename T>
		requires std::is_base_of_v<impl::EventBase, std::remove_reference_t<T>>
	EventDispatcher(T&& evt) : e_{ evt } {}

	bool IsHandled() const {
		return e_.event_handled_;
	}

	template <typename T, typename Fn>
	void Dispatch(Fn&& fn) {
		if (e_.event_handled_) {
			return;
		}
		if (e_.Type() != T::event_id_) {
			return;
		}

		using Ret = std::invoke_result_t<Fn, T&>;

		if constexpr (std::is_same_v<Ret, bool>) {
			if (fn(static_cast<T&>(e_))) {
				e_.event_handled_ = true;
			}
		} else if constexpr (std::is_same_v<Ret, void>) {
			fn(static_cast<T&>(e_));
			// void -> implicitly "not handled"
		}
	}

private:
	operator impl::EventBase&() const {
		return e_;
	}

	impl::EventBase& e_;
};

} // namespace ptgn