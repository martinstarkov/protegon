#pragma once

#include <optional>
#include <vector>

#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/util/concepts.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
class SceneContext;

namespace impl {

struct EntityEvent {
	std::optional<Entity> entity;
	impl::EventData event;
};

} // namespace impl

class LocalEventHandler {
public:
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void Push(Entity entity, TArgs&&... args) {
		if (!entity) {
			return;
		}
		auto event{ impl::EventData::Create<T>(std::forward<TArgs>(args)...) };
		entity_event_queue_.emplace_back(entity, std::move(event));
	}

	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
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