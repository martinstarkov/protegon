#pragma once

#include <memory>
#include <type_traits>
#include <variant>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/util/concepts.h"

namespace ptgn {

namespace impl {

template <typename F, typename T>
concept EventFunctionWithArg = InvocableR<F, void, T&> || InvocableR<F, bool, T&> ||
							   InvocableR<F, void, const T&> || InvocableR<F, bool, const T&>;

template <typename F>
concept EventFunctionNoArg = InvocableR<F, void> || InvocableR<F, bool>;

template <typename TVariant, typename... TArgs>
void VisitAndInvoke(TVariant&& callback_variant, TArgs&&... args) {
	std::visit(
		[&]<typename TCallback>(const TCallback& callback) {
			if constexpr (std::is_invocable_v<TCallback, TArgs...>) {
				callback(std::forward<TArgs>(args)...);
			} else if constexpr (std::is_invocable_v<TCallback>) {
				callback();
			} else {
				static_assert(false, "Callback cannot be invoked with these arguments");
			}
		},
		std::forward<TVariant>(callback_variant)
	);
}

template <typename F, typename T>
concept EventFunctionType = impl::EventFunctionWithArg<F, T> || impl::EventFunctionNoArg<F>;

} // namespace impl

/// @brief Dispatcher for routing events to matching handlers.
///
/// If a handler returns true, the event is marked as handled, otherwise the event keeps
/// propagating.
///
/// Example:
/// @code
/// void OnEvent(EventDispatcher dispatcher) {
///     dispatcher.Dispatch<WindowResized>([](const auto& event) {
///         DoStuff(event.size);
///     });
/// }
/// @endcode
class EventDispatcher {
public:
	EventDispatcher(impl::EventData& event) : event_{ event } {} // NOSONAR

	template <EventType T>
	EventDispatcher(T& event) : event_{ event } {} // NOSONAR

	/// @brief Dispatches the event to the given callable if types match.
	///
	/// If the callback returns `true`, the event is marked handled.
	/// If the callback does not return a bool, the event keeps propagating.
	template <EventType T, impl::EventFunctionType<T> TEventFn>
	void Dispatch(TEventFn&& fn) {
		if (event_.handled) {
			return;
		}

		if (!IsType<T>()) {
			return;
		}

		if constexpr (!std::is_empty_v<T>) {
			auto& payload = std::get<impl::PayloadEvent>(event_.storage);

			PTGN_ASSERT(payload.event);

			T& value{ static_cast<T&>(*payload.event) };
			InvokeHandler<T>(std::forward<TEventFn>(fn), value);
		} else {
			T dummy{};
			InvokeHandler<T>(std::forward<TEventFn>(fn), dummy);
		}
	}

	/// @brief Dispatches to a member functions.
	template <EventType T, typename TObject, typename TMemFn>
	void Dispatch(TMemFn memfn, TObject* obj) {
		Dispatch<T>([obj, memfn]<typename... TArgs>(TArgs&&... args) -> decltype(auto) {
			if constexpr (std::is_invocable_v<TMemFn, TObject*, TArgs...>) {
				return std::invoke(memfn, obj, std::forward<TArgs>(args)...);
			} else if constexpr (std::is_invocable_v<TMemFn, TObject*>) {
				return std::invoke(memfn, obj);
			} else {
				static_assert(false, "Member function cannot be invoked with these arguments");
			}
		});
	}

	template <EventType T, typename TVariant>
	void DispatchVariant(TVariant&& callback_variant) {
		Dispatch<T>([callback = std::forward<TVariant>(callback_variant
					 )]<typename... TEventArgs>(TEventArgs&&... event_args) mutable {
			impl::VisitAndInvoke(callback, std::forward<TEventArgs>(event_args)...);
		});
	}

	template <EventType T, typename TVariant, typename... TBoundArgs>
	void DispatchVariantBound(TVariant&& callback_variant, TBoundArgs&&... bound_args) {
		Dispatch<T>([callback = std::forward<TVariant>(callback_variant),
					 ... args = std::forward<TBoundArgs>(bound_args)]() mutable {
			impl::VisitAndInvoke(callback, args...);
		});
	}

	operator impl::EventData&() const { // NOSONAR
		return event_;
	}

	/// @return True if the event has been handled.
	[[nodiscard]] bool IsHandled() const {
		return event_.handled;
	}

	/// @return True if the stored event is of type `T`.
	[[nodiscard]] constexpr bool IsType(std::size_t event_id) const {
		return std::visit(
			[event_id]<typename TStorage>(const TStorage& storage) -> bool {
				if constexpr (std::is_same_v<TStorage, impl::TagEvent>) {
					return storage.type_id == event_id;
				} else if constexpr (std::is_same_v<TStorage, impl::PayloadEvent>) {
					return storage.event && storage.event->Type() == event_id;
				} else {
					static_assert(false, "Incomplete visitor");
				}
			},
			event_.storage
		);
	}

	/// @return True if the stored event is of type `T`.
	template <EventType T>
	[[nodiscard]] constexpr bool IsType() const {
		return IsType(T::event_id_);
	}

private:
	template <typename T, impl::EventFunctionType<T> TEventFn>
	void InvokeHandler(TEventFn&& fn, T& value) {
		if constexpr (std::is_invocable_r_v<bool, TEventFn, T&>) {
			if (std::forward<TEventFn>(fn)(value)) {
				event_.handled = true;
			}
		} else if constexpr (std::is_invocable_r_v<void, TEventFn, T&> ||
							 std::is_invocable_r_v<void, TEventFn, const T&>) {
			std::forward<TEventFn>(fn)(value);
		} else if constexpr (std::is_invocable_r_v<bool, TEventFn>) {
			if (std::forward<TEventFn>(fn)()) {
				event_.handled = true;
			}
		} else if constexpr (std::is_invocable_r_v<void, TEventFn>) {
			std::forward<TEventFn>(fn)();
		} else {
			static_assert(
				false, "Dispatch handler must be callable as fn(T&) or fn(), returning void or bool"
			);
		}
	}

	impl::EventData& event_;
};

} // namespace ptgn