#include "runtime/event/event_handler.h"

#include <memory>

#include "core/event/dispatcher.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

EventHandler::EventHandler(SceneManager& scenes) : scenes_{ scenes } {}

bool EventHandler::IsInternalEvent(const EventDispatcher& d) const {
	return d.IsType<impl::InternalGameResized>() || d.IsType<impl::InternalDisplayResized>() ||
		   d.IsType<impl::InternalDisplayViewportChanged>();
}

void EventHandler::Emit(EventDispatcher d) {
	for (const auto& entry : scenes_.scenes_) {
		// Skip events during a transition unless they are internal.
		if (!IsInternalEvent(d)) {
			if (entry->transition_ && !entry->transition_->started_) {
				continue;
			}
		}
		entry->InternalEmit(d);
		if (d.IsHandled()) {
			return;
		}
	}
}

} // namespace ptgn