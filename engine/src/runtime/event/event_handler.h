#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <vector>

#include "core/event/event.h"
#include "core/util/concepts.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

class Scene;
class Application;

namespace event {

struct InternalGameResized;
struct InternalDisplayResized;

} // namespace event

namespace impl {

class TweenData;

template <typename T>
concept InternalEvent = IsAnyOf<T, event::InternalGameResized, event::InternalDisplayResized>;

struct QueuedEvent {
	std::optional<Entity> entity;
	impl::EventData event;
};

} // namespace impl

class EventQueue {
public:
	template <EventType T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void Push(const std::optional<Entity>& entity, TArgs&&... args) {
		if (entity.has_value() && !*entity) {
			return;
		}
		queue_.emplace_back(entity, impl::EventData{ T{ std::forward<TArgs>(args)... } });
	}

	bool operator==(const EventQueue&) const = default;

private:
	friend class Scene;
	friend class impl::TweenData;

	std::vector<impl::QueuedEvent> queue_;
};

class EventHandler {
public:
	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void Push(TArgs&&... args) {
		for (const auto& entry : scene_manager_.GetScenes()) {
			// Dont push events during a transition unless they are internal.
			if constexpr (!impl::InternalEvent<T>) {
				if (entry->IsTransitioning()) {
					continue;
				}
			}
			entry->ctx().event.Push<T>(std::nullopt, std::forward<TArgs>(args)...);
		}
	}

private:
	friend class Application;

	explicit EventHandler(SceneManager& scenes);
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	SceneManager& scene_manager_;
};

} // namespace ptgn