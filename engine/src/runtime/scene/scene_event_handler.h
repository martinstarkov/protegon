#pragma once

#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "core/util/concepts.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Application;
class Scene;
class SceneContext;

namespace impl {

struct EntityEvent {
	Entity entity;
	EventData event;
};

} // namespace impl

/// @brief Owns events targeted at a single scene and its entities.
/// The application snapshots these queues before delivering any events.
class LocalEventHandler {
public:
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void Push(Entity entity, TArgs&&... args) {
		if (!entity || !entity.HasScene() || &entity.GetScene() != scene_) {
			return;
		}

		entity_event_queue_.emplace_back(
			entity, impl::EventData::Create<T>(std::forward<TArgs>(args)...)
		);
	}

	/// @brief Deliver only to the owning scene's scripts and OnEvent.
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void PushScene(TArgs&&... args) {
		scene_event_queue_.emplace_back(impl::EventData::Create<T>(std::forward<TArgs>(args)...));
	}

	/// @brief Broadcast to the application's eligible runtime scenes.
	template <typename T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	void PushGlobal(TArgs&&... args) {
		event_handler_.Push<T>(std::forward<TArgs>(args)...);
	}

private:
	friend class Application;
	friend class Scene;
	friend class SceneContext;

	explicit LocalEventHandler(EventHandler& event_handler, Scene& scene) :
		event_handler_{ event_handler }, scene_{ &scene } {}

	~LocalEventHandler() noexcept							   = default;
	LocalEventHandler(const LocalEventHandler&)				   = delete;
	LocalEventHandler& operator=(const LocalEventHandler&)	   = delete;
	LocalEventHandler(LocalEventHandler&&) noexcept			   = delete;
	LocalEventHandler& operator=(LocalEventHandler&&) noexcept = delete;

	void Rebind(Scene& scene) {
		scene_ = &scene;
	}

	EventHandler& event_handler_;
	Scene* scene_{ nullptr };
	std::vector<impl::EntityEvent> entity_event_queue_;
	std::vector<impl::EventData> scene_event_queue_;
};

} // namespace ptgn
