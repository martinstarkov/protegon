#pragma once

#include <concepts>
#include <optional>
#include <vector>

#include "core/event/event.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
class SceneContext;
class Application;

namespace impl {

struct EntityEvent {
	std::optional<Entity> entity;
	impl::EventData event;
};

} // namespace impl

class EventHandler {
public:
	template <typename T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void Push(TArgs&&... args) {
		global_event_queue_.emplace_back(
			Hash<T>(), false, std::make_unique<T>(std::forward<TArgs>(args)...)
		);
	}

private:
	friend class Application;

	EventHandler()									 = default;
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	std::vector<impl::EventData> global_event_queue_;
};

class LocalEventHandler {
public:
	template <typename T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void Push(Entity entity, TArgs&&... args) {
		if (!entity) {
			return;
		}
		entity_event_queue_.emplace_back(entity, std::forward<TArgs>(args)...);
	}

	template <typename T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushGlobal(TArgs&&... args) {
		event_handler_.Push<T>(std::forward<TArgs>(args)...);
	}

private:
	friend class Scene;
	friend class SceneContext;

	explicit LocalEventHandler(EventHandler& event_handler) : event_handler_{ event_handler } {}

	~LocalEventHandler() noexcept							   = default;
	LocalEventHandler(const LocalEventHandler&)				   = delete;
	LocalEventHandler& operator=(const LocalEventHandler&)	   = delete;
	LocalEventHandler(LocalEventHandler&&) noexcept			   = delete;
	LocalEventHandler& operator=(LocalEventHandler&&) noexcept = delete;

	EventHandler& event_handler_;

	std::vector<impl::EntityEvent> entity_event_queue_;
};

} // namespace ptgn