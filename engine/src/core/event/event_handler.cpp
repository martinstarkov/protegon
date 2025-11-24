#include "core/event/event_handler.h"

#include <memory>

#include "core/event/event.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn {

EventHandler::EventHandler(SceneManager& scenes) : scenes_{ scenes } {}

void EventHandler::Emit(EventDispatcher d) {
	for (auto& entry : scenes_.entries_) {
		entry.ptr->InternalEmit(d);
		if (d.IsHandled()) {
			break;
		}
	}
}

} // namespace ptgn