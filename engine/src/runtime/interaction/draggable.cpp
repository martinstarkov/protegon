#include "runtime/interaction/draggable.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

void SetDraggable(Entity entity, bool enabled) {
	entity.TryAdd<impl::Draggable>().enabled = enabled;
}

bool IsDraggable(Entity entity) {
	return entity.Has<impl::Draggable>() && entity.Get<impl::Draggable>().enabled;
}

void SetDraggableFollowMouse(Entity draggable, bool follow_mouse) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot make entity without Draggable component follow mouse");
		return;
	}
	draggable.Get<impl::Draggable>().follow_mouse = follow_mouse;
}

bool DoesDraggableFollowMouse(Entity draggable) {
	return draggable.Has<impl::Draggable>() && draggable.Get<impl::Draggable>().follow_mouse;
}

bool IsDragging(Entity entity) {
	const auto& dragging_entities{ entity.GetScene().ctx().interaction.dragging_entities_ };
	return std::ranges::any_of(dragging_entities, [entity](const auto& entry) {
		return std::ranges::contains(entry.second.entities, entity);
	});
}

V2_float GetDragOffset(Entity draggable) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot get drag offset of entity without Draggable component");
		return {};
	}
	return draggable.Get<impl::Draggable>().offset;
}

V2_float GetDragStart(Entity draggable) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot get drag start of entity without Draggable component");
		return {};
	}
	return draggable.Get<impl::Draggable>().start;
}

bool IsBeingDragged(Entity draggable) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot check if entity without Draggable component is being dragged");
		return false;
	}
	return draggable.Get<impl::Draggable>().dragging;
}

std::vector<Entity> GetDropzones(Entity draggable) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot get dropzones onto which entity without Draggable component is dropped");
		return {};
	}
	return draggable.Get<impl::Draggable>().dropzones;
}

std::vector<Entity> GetHoveredDropzones(Entity draggable) {
	if (!draggable.Has<impl::Draggable>()) {
		PTGN_WARN("Cannot get dropzones over which entity without Draggable component is hovered");
		return {};
	}
	return draggable.Get<impl::Draggable>().hovered_dropzones;
}

void SetDraggableCondition(Entity draggable, DragEventPhase phase, TriggerCondition condition) {
	impl::SetTriggerCondition<impl::Draggable>(draggable, phase, condition);
}

} // namespace ptgn
