#include "runtime/ui/interactive.h"

#include <algorithm>
#include <optional>
#include <ostream>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_input.h"

namespace ptgn {

template <typename TComponent>
void SetComponentState(Entity entity, ComponentState state) {
	switch (state) {
		using enum ptgn::ComponentState;
		case Disabled: entity.TryAdd<TComponent>().enabled = false; break;
		case Enabled:  entity.TryAdd<TComponent>().enabled = true; break;
		case Removed:  entity.Remove<TComponent>(); break;
		default:	   PTGN_ERROR("Invalid ComponentState");
	}
}

template <typename TComponent>
bool IsEnabled(Entity entity) {
	return entity.Has<TComponent>() && entity.Get<TComponent>().enabled;
}

void SetInteractive(Entity entity, ComponentState state) {
	SetComponentState<impl::Interactive>(entity, state);
}

bool IsInteractive(Entity entity) {
	return IsEnabled<impl::Interactive>(entity);
}

void AddInteractiveShape(
	Entity entity, GameObject&& shape, std::optional<std::string_view> shape_id,
	bool ignore_parent_transform
) {
	IgnoreParentTransform(shape, ignore_parent_transform);
	SetInteractive(entity);
	if (shape_id.has_value()) {
		PTGN_ASSERT(
			!HasChild(entity, *shape_id),
			"Cannot add the same named interactable to an entity more than once"
		);
	}
	AddChild(entity, shape, shape_id);
	auto& interactive{ entity.Get<impl::Interactive>() };
	interactive.shapes.emplace_back(std::move(shape));
}

void SetInteractiveShape(
	Entity entity, GameObject&& shape, std::optional<std::string_view> shape_id,
	bool ignore_parent_transform
) {
	ClearInteractiveShapes(entity);
	AddInteractiveShape(entity, std::move(shape), shape_id, ignore_parent_transform);
}

void RemoveInteractiveShape(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Interactive>()) {
		return;
	}
	if (!HasChild(entity, name)) {
		return;
	}
	Entity child{ GetChild(entity, name) };
	auto& interactive{ entity.Get<impl::Interactive>() };
	std::erase(interactive.shapes, child);
}

bool HasInteractiveShape(Entity entity) {
	if (!entity.Has<impl::Interactive>()) {
		return false;
	}
	const auto& interactive{ entity.Get<impl::Interactive>() };
	return !interactive.shapes.empty() || entity.HasAny<Rect, Circle>();
}

bool HasInteractiveShape(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Interactive>()) {
		return false;
	}
	if (!HasChild(entity, name)) {
		return false;
	}

	Entity child{ GetChild(entity, name) };

	const auto& interactive{ entity.Get<impl::Interactive>() };

	return std::ranges::contains(interactive.shapes, child);
}

std::vector<Entity> GetInteractiveShapes(Entity entity) {
	PTGN_ASSERT(entity.Has<impl::Interactive>());
	const auto& interactive{ entity.Get<impl::Interactive>() };
	std::vector<Entity> interactables;
	interactables.reserve(interactive.shapes.size());
	for (const auto& shape : interactive.shapes) {
		interactables.emplace_back(shape);
	}
	return interactables;
}

void ClearInteractiveShapes(Entity entity) {
	if (!entity.Has<impl::Interactive>()) {
		return;
	}
	auto& interactive{ entity.Get<impl::Interactive>() };
	// Clear owned entities.
	interactive.shapes.clear();
}

void SetDraggable(Entity entity, ComponentState state) {
	SetComponentState<impl::Draggable>(entity, state);
}

bool IsDraggable(Entity entity) {
	return IsEnabled<impl::Draggable>(entity);
}

bool IsDragging(Entity entity) {
	const auto& dragging_entities{ entity.GetScene().ctx().input.dragging_entities_ };
	for (const auto& [camera, entities] : dragging_entities) {
		if (entities.contains(entity)) {
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

void SetDropzone(Entity entity, ComponentState state) {
	SetComponentState<impl::Dropzone>(entity, state);
}

bool IsDropzone(Entity entity) {
	return IsEnabled<impl::Dropzone>(entity);
}

const std::unordered_set<Entity>& GetDropzones(Entity draggable) {
	return draggable.Get<impl::Draggable>().dropzones;
}

const std::unordered_set<Entity>& GetDraggables(Entity dropzone) {
	return dropzone.Get<impl::Dropzone>().draggables;
}

template <typename TComponent>
void SetCondition(Entity entity, DragEventPhase phase, TriggerCondition condition) {
	auto& component{ entity.Get<TComponent>() };
	switch (phase) {
		using enum ptgn::DragEventPhase;
		case MoveOver: component.move_condition = condition; break;
		case Drop:	   component.drop_condition = condition; break;
		case Pickup:   component.pickup_condition = condition; break;
		default:	   PTGN_ERROR("Unhandled DragEventPhase");
	}
}

void SetDraggableCondition(Entity draggable, DragEventPhase phase, TriggerCondition condition) {
	SetCondition<impl::Draggable>(draggable, phase, condition);
}

void SetDropzoneCondition(Entity dropzone, DragEventPhase phase, TriggerCondition condition) {
	SetCondition<impl::Dropzone>(dropzone, phase, condition);
}

std::ostream& operator<<(std::ostream& os, ComponentState state) {
	switch (state) {
		using enum ComponentState;
		case Disabled: return os << "Disabled";
		case Enabled:  return os << "Enabled";
		case Removed:  return os << "Removed";
		default:	   PTGN_ERROR("Unknown ComponentState: ", std::to_underlying(state));
	}
}

std::ostream& operator<<(std::ostream& os, TriggerCondition condition) {
	switch (condition) {
		using enum TriggerCondition;
		case None:				return os << "None";
		case MouseOverlaps:		return os << "MouseOverlaps";
		case TransformOverlaps: return os << "TransformOverlaps";
		case Overlaps:			return os << "Overlaps";
		case Contains:			return os << "Contains";
		default:				return os << "Unknown TriggerCondition: " << std::to_underlying(condition);
	}
}

std::ostream& operator<<(std::ostream& os, DragEventPhase phase) {
	switch (phase) {
		using enum DragEventPhase;
		case MoveOver: return os << "MoveOver";
		case Drop:	   return os << "Drop";
		case Pickup:   return os << "Pickup";
		default:	   PTGN_ERROR("Unknown DragEventPhase: ", std::to_underlying(phase));
	}
}

} // namespace ptgn