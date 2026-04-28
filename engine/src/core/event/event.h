#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"

namespace ptgn {

class Scene;
class Application;

namespace impl {

class TweenData;

struct EventData {
	using Payload = std::unique_ptr<void, void (*)(void*)>;

	std::size_t type_hash{ 0 };
	bool handled{ false };
	Payload payload{ nullptr, +[](void*) {
					} };

	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	static EventData Create(TArgs&&... args) {
		if constexpr (sizeof...(TArgs) == 0 && std::is_empty_v<T>) {
			return EventData{ Hash<T>(), false, { nullptr, +[](void*) {
												 } } };
		} else {
			auto payload{ new T{ std::forward<TArgs>(args)... } };
			return EventData{ Hash<T>(), false, Payload{ payload, +[](void* ptr) {
															delete static_cast<T*>(ptr);
														} } };
		}
	}
};

template <typename F, typename T>
concept EventFunctionWithArg = InvocableR<F, void, T&> || InvocableR<F, bool, T&> ||
							   InvocableR<F, void, const T&> || InvocableR<F, bool, const T&>;

template <typename F>
concept EventFunctionNoArg = InvocableR<F, void> || InvocableR<F, bool>;

template <typename TVariant, typename... TArgs>
bool VisitAndInvoke(TVariant&& callback_variant, TArgs&&... args) {
	return std::visit(
		[&]<typename TCallback>(TCallback& callback) -> bool {
			if constexpr (std::is_invocable_v<TCallback, TArgs...>) {
				if constexpr (std::is_same_v<std::invoke_result_t<TCallback&, TArgs...>, bool>) {
					return callback(std::forward<TArgs>(args)...);
				} else {
					callback(std::forward<TArgs>(args)...);
					return false;
				}
			} else {
				if constexpr (std::is_same_v<std::invoke_result_t<TCallback&>, bool>) {
					return callback();
				} else {
					callback();
					return false;
				}
			}
		},
		std::forward<TVariant>(callback_variant)
	);
}

template <typename F, typename T>
concept EventFunctionType = impl::EventFunctionWithArg<F, T> || impl::EventFunctionNoArg<F>;

} // namespace impl

/// @brief Object for routing events to matching handlers.
///
/// If a handler returns true, the event is marked as handled, otherwise the event keeps
/// propagating.
///
/// Example:
/// @code
/// void OnEvent(Event e) {
///     e.Dispatch<WindowResized>([](const auto& event) {
///         DoStuff(event.size);
///     });
/// }
/// @endcode
class Event {
public:
	/// @brief Dispatches the event to the given callable if types match.
	///
	/// If the callback returns `true`, the event is marked handled.
	/// If the callback does not return a bool, the event keeps propagating.
	template <typename T, typename TEventFn>
	void Dispatch(TEventFn&& fn) {
		if (event_.handled) {
			return;
		}

		if (!IsType<T>()) {
			return;
		}

		if constexpr (!std::is_empty_v<T>) {
			PTGN_ASSERT(event_.payload, "Payload for non empty event must be set");
			T& value{ *static_cast<T*>(event_.payload.get()) };
			InvokeHandler<T>(std::forward<TEventFn>(fn), value);
		} else {
			if constexpr (std::is_invocable_r_v<bool, TEventFn>) {
				if (std::forward<TEventFn>(fn)()) {
					event_.handled = true;
				}
			} else if constexpr (std::is_invocable_r_v<void, TEventFn>) {
				std::forward<TEventFn>(fn)();
			} else {
				static_assert(
					false, "Dispatch handler for empty event must be callable with no args fn(), "
						   "returning void or bool"
				);
			}
		}
	}

	/// @brief Dispatches to a member functions.
	template <typename T, typename TObject, typename TMemFn>
	void Dispatch(TMemFn memfn, TObject* obj) {
		Dispatch<T>(
			[obj, memfn]<typename... TArgs>([[maybe_unused]] TArgs&&... args) -> decltype(auto) {
				if constexpr (std::is_invocable_v<TMemFn, TObject*, TArgs...>) {
					return std::invoke(memfn, obj, std::forward<TArgs>(args)...);
				} else if constexpr (std::is_invocable_v<TMemFn, TObject*>) {
					return std::invoke(memfn, obj);
				} else {
					static_assert(false, "Member function cannot be invoked with these arguments");
				}
			}
		);
	}

	template <typename T, typename TVariant>
	void DispatchVariant(TVariant&& callback_variant) {
		Dispatch<T>(
			[callback = std::forward<TVariant>(callback_variant
			 )]<typename... TEventArgs>(TEventArgs&&... event_args) mutable -> bool {
				return impl::VisitAndInvoke(callback, std::forward<TEventArgs>(event_args)...);
			}
		);
	}

	template <typename T, typename TVariant, typename... TBoundArgs>
	void DispatchVariantBound(TVariant&& callback_variant, TBoundArgs&&... bound_args) {
		Dispatch<T>(
			[callback = std::forward<TVariant>(callback_variant),
			 ... args = std::forward<TBoundArgs>(bound_args)]() mutable -> bool {
				return impl::VisitAndInvoke(callback, args...);
			}
		);
	}

	/// @return True if the event has been handled.
	[[nodiscard]] bool IsHandled() const {
		return event_.handled;
	}

	/// @return True if the stored event has the same type hash as the given type hash.
	[[nodiscard]] constexpr bool IsType(std::size_t type_hash) const {
		return event_.type_hash == type_hash;
	}

	/// @return True if the stored event is of type `T`.
	template <typename T>
	[[nodiscard]] constexpr bool IsType() const {
		return event_.type_hash == Hash<T>();
	}

private:
	friend class Application;
	friend class Scene;
	friend class impl::TweenData;

	explicit Event(impl::EventData& event) : event_{ event } {}

	template <typename T, impl::EventFunctionType<T> TEventFn>
	void InvokeHandler(TEventFn&& fn, [[maybe_unused]] T& value) {
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

template <typename T>
using EventCallback = std::variant<
	std::function<void()>, std::function<void(T&)>, std::function<void(const T&)>,
	std::function<bool()>, std::function<bool(T&)>, std::function<bool(const T&)>>;

template <typename F, typename T>
concept EventCallbackInvocable =
	InvocableR<std::remove_cvref_t<F>&, bool, T&> ||
	InvocableR<std::remove_cvref_t<F>&, void, T&> ||
	InvocableR<std::remove_cvref_t<F>&, bool, const T&> ||
	InvocableR<std::remove_cvref_t<F>&, void, const T&> ||
	InvocableR<std::remove_cvref_t<F>&, bool> || InvocableR<std::remove_cvref_t<F>&, void>;

namespace impl {

template <typename T, EventCallbackInvocable<T> F>
EventCallback<T> MakeEventCallback(F&& f) {
	using Fn = std::remove_cvref_t<F>;

	if constexpr (std::is_invocable_r_v<bool, Fn&, T&>) {
		return std::function<bool(T&)>{ std::forward<F>(f) };
	} else if constexpr (std::is_invocable_r_v<void, Fn&, T&>) {
		return std::function<void(T&)>{ std::forward<F>(f) };
	} else if constexpr (std::is_invocable_r_v<bool, Fn&, const T&>) {
		return std::function<bool(const T&)>{ std::forward<F>(f) };
	} else if constexpr (std::is_invocable_r_v<void, Fn&, const T&>) {
		return std::function<void(const T&)>{ std::forward<F>(f) };
	} else if constexpr (std::is_invocable_r_v<bool, Fn&>) {
		return std::function<bool()>{ std::forward<F>(f) };
	} else if constexpr (std::is_invocable_r_v<void, Fn&>) {
		return std::function<void()>{ std::forward<F>(f) };
	} else {
		static_assert(false, "Unsupported event callback");
	}
}

} // namespace impl

} // namespace ptgn