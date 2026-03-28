#include "runtime/event/event_handler.h"

#include <memory>

#include "core/event/dispatcher.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

EventHandler::EventHandler(SceneManager& scenes) : scenes_{ scenes } {}

void EventHandler::Emit(EventDispatcher d) {
	for (const auto& entry : scenes_.scenes_) {
		if (entry->transition_ && !entry->transition_->started_) {
			continue;
		}
		entry->InternalEmit(d);
		if (d.IsHandled()) {
			return;
		}
	}
}

} // namespace ptgn