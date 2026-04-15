#include "runtime/interaction/dropzone.h"

#include <vector>

#include "runtime/ecs/entity.h"
#include "runtime/interaction/trigger_condition.h"

namespace ptgn {

void SetDropzone(Entity entity, ComponentState state) {
	impl::SetComponentState<impl::Dropzone>(entity, state);
}

bool IsDropzone(Entity entity) {
	return entity.Has<impl::Dropzone>() && entity.Get<impl::Dropzone>().enabled;
}

std::vector<Entity> GetDraggables(Entity dropzone) {
	return dropzone.Get<impl::Dropzone>().draggables;
}

void SetDropzoneCondition(Entity dropzone, DragEventPhase phase, TriggerCondition condition) {
	impl::SetTriggerCondition<impl::Dropzone>(dropzone, phase, condition);
}

} // namespace ptgn