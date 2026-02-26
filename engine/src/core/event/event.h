#pragma once

#include <concepts>
#include <type_traits>

#include "core/util/hash.h"
#include "core/util/type_info.h"

namespace ptgn {

class EventDispatcher;

namespace impl {

struct EventBase {
public:
	virtual ~EventBase() = default;

private:
	friend class ptgn::EventDispatcher;

	bool event_handled_{ false };

	constexpr virtual std::size_t Type() = 0;
};

} // namespace impl

/// @brief CRTP base class for strongly-typed events.
template <typename Derived>
struct Event : public impl::EventBase {
private:
	friend class EventDispatcher;

	static constexpr std::size_t event_id_{ Hash(type_name<Derived>()) };

	constexpr std::size_t Type() override {
		return Hash(type_name<Derived>());
	}
};

template <typename T>
concept EventType = std::derived_from<T, Event<T>> && std::is_base_of_v<impl::EventBase, T>;

} // namespace ptgn