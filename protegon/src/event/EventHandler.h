#pragma once

#include <cstdint> // std::uint32_t
#include <unordered_map> // std::unordered_map
#include <cassert> // assert
#include <vector> // std::vector

#include "ecs/ECS.h"
#include "utils/TypeTraits.h"

namespace engine {

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

class EventHandler {
public:
	// Register an event class which implements an Invoke function taking an entity as an argument.
	template <typename T>
	static void Register(const ecs::Entity& entity) {
		static_assert(type_traits::has_static_invoke<T>, "Cannot register event which does not implement a static method with the name 'Invoke'");
		auto event_id{ GetEventId<T>() };
		auto caller_it{ callers_.find(entity) };
		if (caller_it == std::end(callers_)) {
			callers_.emplace(std::move(entity), std::move(std::vector<EventId>{ event_id }));
		} else {
			caller_it->second.emplace_back(event_id);
		}
		auto event_it{ events_.find(event_id) };
		if (event_it == std::end(events_)) {
			//static_assert(std::is_convertible_v<&T::Invoke, EventFunction>, "Could not register invoke function which does not take in a const ecs::Entity& as the only argument");
			auto invoke_function{ internal::EventCast<T>() };
			assert(invoke_function != nullptr && "Could not create valid event invoke function pointer");
			events_.emplace(event_id, invoke_function);
		}
	}
	// Invoke all events registered under a given entity.
	static void Invoke(ecs::Entity& entity) {
		auto caller_it{ callers_.find(entity) };
		assert(caller_it != std::end(callers_) && "Could not invoke event on entity which has not registered such an event");
		for (auto event_id : caller_it->second) {
			auto it{ events_.find(event_id) };
			assert(it != std::end(events_) && "Could not find valid event invoke function pointer");
			auto invoke_function{ it->second };
			assert(invoke_function != nullptr && "Could not create valid event invoke function pointer");
			invoke_function(entity);
		}
	}
	static void Remove(const ecs::Entity& entity) {
		callers_.erase(entity);
	}
private:
	using EventId = std::uint32_t;
	template <typename T>
	static EventId GetEventId() {
		static EventId id{ EventTypeCount()++ };
		return id;
	}
	static EventId& EventTypeCount() {
		static EventId id{ 0 };
		return id;
	}
	static std::unordered_map<ecs::Entity, std::vector<EventId>> callers_;
	static std::unordered_map<EventId, internal::EventFunction> events_;
};

} // namespace engine

