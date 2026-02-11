#include "runtime/event/event_handler.h"

#include <memory>

#include "core/event/dispatcher.h"
#include "platform/input/events.h"
#include "renderer/renderer.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

namespace ptgn {

EventHandler::EventHandler(SceneManager& scenes, Renderer& renderer) :
	scenes_{ scenes }, renderer_{ renderer } {}

void EventHandler::Emit(EventDispatcher d) {
	renderer_.OnEvent(d);

	for (auto& entry : scenes_.entries_) {
		entry.ptr->InternalEmit(d);
		if (d.IsHandled()) {
			return;
		}
	}
}

} // namespace ptgn