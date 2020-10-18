#pragma once

#include <cstdint>
#include <unordered_map>
#include <cassert>
#include <memory>
#include <type_traits>
#include <functional>

#include <engine/ecs/ECS.h>

namespace engine {

namespace detail {

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

template <typename T>
constexpr auto has_invoke_helper(const T&, int)
-> decltype(&T::Invoke, &T::Invoke);

template <typename T>
constexpr void* has_invoke_helper(const T&, long) {
	return nullptr;
}

template <typename T>
bool constexpr has_invoke = !std::is_same<decltype(has_invoke_helper(std::declval<T>(), 0)), void*>::value;

template <typename T>
bool constexpr has_static_invoke = has_invoke<T> && !std::is_member_function_pointer_v<decltype(has_invoke_helper(std::declval<T>(), 0))>;

template <typename T, std::enable_if_t<has_static_invoke<T>, int> = 0>
constexpr EventFunction EventCast() {
	return static_cast<EventFunction>(static_cast<void*>(&T::Invoke));
}

template <typename T, std::enable_if_t<!has_invoke<T> || std::is_member_function_pointer_v<decltype(has_invoke_helper(std::declval<T>(), 0))>, int> = 0>
constexpr EventFunction EventCast() {
	return nullptr;
}

// source: https://stackoverflow.com/a/8645270
template <typename ...TArgs>
constexpr std::size_t Arity(void (*)(TArgs...)) {
	return sizeof...(TArgs);
}

template <typename T, std::enable_if_t<has_static_invoke<T>, int> = 0>
constexpr std::size_t EventArgumentCount() {
	return Arity(&T::Invoke);
}

template <typename T, std::enable_if_t<!has_invoke<T> || std::is_member_function_pointer_v<decltype(has_invoke_helper(std::declval<T>(), 0))>, int> = 0>
constexpr std::size_t EventArgumentCount() {
	return 0;
}

} // namespace detail

class EventHandler {
public:
	template <typename T>
	static void Register(ecs::Entity entity) {
		static_assert(detail::has_static_invoke<T>, "Cannot register event which does not implement a static method with the name 'Invoke'");
		//static_assert("Cannot register event which does not implement a static method with the name 'Invoke'");
		auto event_id = detail::GetEventId<T>();
		auto caller_it = callers.find(entity);
		if (caller_it == std::end(callers)) {
			callers.emplace(std::move(entity), std::move(std::vector<detail::EventId>{ event_id }));
		} else {
			caller_it->second.emplace_back(event_id);
		}
		auto event_it = events.find(event_id);
		if (event_it == std::end(events)) {
			auto function_pointer = detail::EventCast<T>();
			assert(function_pointer != nullptr && "Could not create valid event invoke function pointer");
			events.emplace(event_id, std::pair<std::size_t, detail::EventFunction>(detail::EventArgumentCount<T>(), function_pointer));
		}
	}
	template <typename ...TArgs>
	static void Invoke(ecs::Entity entity, TArgs&&... args) {
		auto caller_it = callers.find(entity);
		assert(caller_it != std::end(callers) && "Could not invoke event on entity which has not registered such an event");
		for (auto event_id : caller_it->second) {
			auto it = events.find(event_id);
			assert(it != std::end(events) && "Could not find valid event invoke function pointer");
			assert(it->second.first == sizeof...(TArgs) && "Event invoke call argument count does not match the event's invoke function");
			auto function_pointer = reinterpret_cast<void (*)(TArgs &&...)>(it->second.second);
			assert(function_pointer != nullptr && "Could not create valid event invoke function pointer");
			function_pointer(std::forward<TArgs>(args)...);
		}
	}
private:
	static std::unordered_map<ecs::Entity, std::vector<detail::EventId>> callers;
	static std::unordered_map<detail::EventId, std::pair<std::size_t, detail::EventFunction>> events;
};

} // namespace engine

