#pragma once

#include <cstdint> // std::uint32_t
#include <unordered_map> // std::unordered_map
#include <vector> // std::vector

#include "core/ECS.h"
#include "utility/TypeTraits.h"
#include "utility/Singleton.h"
#include <cassert> // assert

namespace ptgn {

namespace internal {

using EventFunction = void (*)(ecs::Entity& invoker);

template <typename T,
	type_traits::has_static_invoke_e<T> = true>
constexpr EventFunction EventCast() {
	return &T::Invoke;
}

template <typename T, typename Function>
constexpr EventFunction EventCast() {
	return nullptr;
}

} // namespace internal

class EventHandler : public Singleton<EventHandler> {
public:
	// Register an event class which implements an Invoke function taking an entity as an argument.
	template <typename TEvent>
	static void Register(const ecs::Entity& invoker) {
		static_assert(type_traits::has_static_invoke<TEvent>, 
					  "Cannot register event which does not implement a static 'Invoke' function");
		auto& instance{ GetInstance() };
		const auto event_id{ GetEventId<TEvent>() };
		auto caller_it{ instance.callers_.find(invoker) };
		if (caller_it == std::end(instance.callers_)) {
			// First time caller.
			instance.callers_.emplace(invoker, std::move(std::vector<EventId>{ event_id }));
		} else {
			caller_it->second.emplace_back(event_id);
		}
		auto event_it{ instance.events_.find(event_id) };
		if (event_it == std::end(instance.events_)) {
			// New event.
			auto invoke_function{ internal::EventCast<TEvent>() };
			assert(invoke_function != nullptr && 
				   "Could not create valid event invoke function pointer");
			instance.events_.emplace(event_id, invoke_function);
		}
	}

	// Invoke all events registered under a given entity.
	static void Invoke(ecs::Entity& invoker);

	// Remove an entity from the event handler.
	static void Remove(const ecs::Entity& invoker);
private:
	friend class Engine;
	friend class Singleton<EventHandler>;
	
	using EventId = std::uint32_t;

	// Refreshes event handler dead entities.
	static void Update();

	// Returns a unique event id for each type.
	template <typename TEvent>
	static EventId GetEventId() {
		static EventId id{ EventTypeCount()++ };
		return id;
	}

	// Generates a new event id.
	static EventId& EventTypeCount();

	EventHandler() = default;
	~EventHandler() = default;

	std::unordered_map<ecs::Entity, std::vector<EventId>> callers_;
	std::unordered_map<EventId, internal::EventFunction> events_;
};

} // namespace ptgn