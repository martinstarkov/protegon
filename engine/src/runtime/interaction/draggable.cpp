#include "runtime/interaction/draggable.h"

#include <algorithm>
#include <vector>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

void SetDraggable(Entity entity, ComponentState state) {
	impl::SetComponentState<impl::Draggable>(entity, state);
}

bool IsDraggable(Entity entity) {
	return entity.Has<impl::Draggable>() && entity.Get<impl::Draggable>().enabled;
}

bool IsDragging(Entity entity) {
	const auto& dragging_entities{ entity.GetScene().ctx().interaction.dragging_entities_ };
	for (const auto& [camera, entities] : dragging_entities) {
		if (std::ranges::contains(entities.entities, entity)) {
			return true;
		}
	}
	return false;
}

V2_float GetDragOffset(Entity draggable) {
	return draggable.Get<impl::Draggable>().offset;
}

V2_float GetDragStart(Entity draggable) {
	return draggable.Get<impl::Draggable>().start;
}

bool IsBeingDragged(Entity draggable) {
	return draggable.Get<impl::Draggable>().dragging;
}

std::vector<Entity> GetDropzones(Entity draggable) {
	return draggable.Get<impl::Draggable>().dropzones;
}

std::vector<Entity> GetHoveredDropzones(Entity draggable) {
	return draggable.Get<impl::Draggable>().hovered_dropzones;
}

void SetDraggableCondition(Entity draggable, DragEventPhase phase, TriggerCondition condition) {
	impl::SetTriggerCondition<impl::Draggable>(draggable, phase, condition);
}

} // namespace ptgn