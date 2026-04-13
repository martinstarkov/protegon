#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <variant>

#include "core/util/hash.h"

namespace ptgn {

class EventDispatcher;

namespace impl {

struct EventBase {
public:
	bool operator==(const EventBase&) const = default;

	virtual ~EventBase() = default;

private:
	friend class ptgn::EventDispatcher;

	bool event_handled_{ false };

	constexpr virtual std::size_t Type() const = 0;
};

} // namespace impl

/// @brief CRTP base class for strongly-typed events.
template <typename Derived>
struct Event : public impl::EventBase {
public:
	static constexpr std::size_t TypeId() {
		return event_id_;
	}

private:
	friend class EventDispatcher;

	static constexpr std::size_t event_id_{ Hash<Derived>() };

	constexpr std::size_t Type() const override {
		return event_id_;
	}
};

template <typename T>
concept EventType = std::derived_from<T, Event<T>> && std::is_base_of_v<impl::EventBase, T>;

namespace impl {

struct TagEvent {
	bool operator==(const TagEvent&) const = default;

	std::size_t type_id{ 0 };
};

struct PayloadEvent {
	bool operator==(const PayloadEvent&) const = default;

	std::unique_ptr<EventBase> event;
};

struct EventData {
	template <EventType T>
	explicit EventData(T&& event) {
		using U = std::remove_cvref_t<T>;
		if constexpr (std::is_empty_v<U>) {
			storage = TagEvent{ Hash<U>() };
		} else {
			storage = PayloadEvent{ std::make_unique<U>(std::forward<T>(event)) };
		}
	}

	std::variant<TagEvent, PayloadEvent> storage;
	bool handled{ false };

	bool operator==(const EventData&) const = default;
};

} // namespace impl

} // namespace ptgn