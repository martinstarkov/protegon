#pragma once

#include <concepts>
#include <memory>
#include <optional>

#include "core/event/event.h"
#include "renderer/primitives/event.h"
#include "runtime/event/event_dispatcher.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

class EventHandler {
public:
	explicit EventHandler(SceneManager& scenes);
	~EventHandler() noexcept						 = default;
	EventHandler(const EventHandler&)				 = delete;
	EventHandler& operator=(const EventHandler&)	 = delete;
	EventHandler(EventHandler&&) noexcept			 = delete;
	EventHandler& operator=(EventHandler&&) noexcept = delete;

	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void Push(TArgs&&... args) {
		for (const auto& entry : scenes_.scenes_) {
			// Dont push events during a transition unless they are internal.
			if constexpr (!impl::InternalRenderEvent<T>) {
				if (entry->transition_ && !entry->transition_->started_) {
					continue;
				}
			}
			entry->ctx().event.Push<T>(std::nullopt, std::forward<TArgs>(args)...);
		}
	}

private:
	SceneManager& scenes_;
};

} // namespace ptgn