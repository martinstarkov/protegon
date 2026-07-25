#include "runtime/interaction/dropzone.h"

#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/interaction/trigger_condition.h"

namespace ptgn {

void SetDropzone(Entity entity, bool enabled) {
	entity.TryAdd<impl::Dropzone>().enabled = enabled;
}

bool IsDropzone(Entity entity) {
	return entity.Has<impl::Dropzone>() && entity.Get<impl::Dropzone>().enabled;
}

std::vector<Entity> GetDraggables(Entity dropzone) {
	if (!dropzone.Has<impl::Dropzone>()) {
		PTGN_WARN("Cannot get draggables currently dropped onto an entity without Dropzone component");
		return {};
	}
	return dropzone.Get<impl::Dropzone>().draggables;
}

void SetDropzoneCondition(Entity dropzone, DragEventPhase phase, TriggerCondition condition) {
	impl::SetTriggerCondition<impl::Dropzone>(dropzone, phase, condition);
}

} // namespace ptgn