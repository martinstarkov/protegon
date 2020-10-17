#pragma once

#include <cstdint>
#include <unordered_map>
#include <cassert>
#include <memory>

#include <engine/ecs/ECS.h>

namespace engine {

using EventId = std::uint32_t;

static EventId event_counter{ 0 };

// Create / retrieve a unique id for each event class.
template <typename T>
static EventId GetEventId() {
	static EventId id = event_counter++;
	return id;
}

template <typename T, typename ...TArgs>
static void EventCall(TArgs&&... args) {
	T{ args... };
}

using EventFunction = void (*)();

template <typename ...TArgs>
using SpecificFunction = void (*)(TArgs&&...);

class EventHandler {
public:
	template <typename T, typename ...TArgs>
	static void Register(ecs::Entity entity) {
		auto event_id = GetEventId<T>();
		auto caller_it = callers.find(entity);
		if (caller_it == std::end(callers)) {
			callers.emplace(std::move(entity), std::move(std::vector<EventId>{ event_id }));
		} else {
			caller_it->second.emplace_back(event_id);
		}
		auto event_it = events.find(event_id);
		if (event_it == std::end(events)) {
			events.emplace(event_id, reinterpret_cast<EventFunction>(&EventCall<T, TArgs...>));
		}
	}
	template <typename ...TArgs>
	static void Invoke(ecs::Entity entity, TArgs&&... args) {
		auto caller_it = callers.find(entity);
		assert(caller_it != std::end(callers) && "Could not invoke event on entity which has not registered such an event");
		for (auto event_id : caller_it->second) {
			auto it = events.find(event_id);
			assert(it != std::end(events) && "Could not find valid event invoke function pointer");
			reinterpret_cast<SpecificFunction<TArgs...>>(it->second)(std::forward<TArgs>(args)...);
		}
	}
private:
	static std::unordered_map<ecs::Entity, std::vector<EventId>> callers;
	static std::unordered_map<EventId, EventFunction> events;
};

} // namespace engine

