#include "runtime/input/interactive.h"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/log.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"

namespace ptgn {

V2_float GetDragOffset(Entity draggable) {
	return draggable.Get<impl::Draggable>().offset;
}

V2_float GetDragStart(Entity draggable) {
	return draggable.Get<impl::Draggable>().start;
}

bool IsBeingDragged(Entity draggable) {
	return draggable.Get<impl::Draggable>().dragging;
}

void SetDraggableCondition(Entity draggable, DragEventPhase phase, TriggerCondition condition) {
	auto& d = draggable.Get<impl::Draggable>();

	switch (phase) {
		using enum ptgn::DragEventPhase;
		case MoveOver: d.move_condition = condition; break;
		case Drop:	   d.drop_condition = condition; break;
		case Pickup:   d.pickup_condition = condition; break;
		default:	   PTGN_ERROR("Unhandled DragEventPhase");
	}
}

void SetDropzoneCondition(Entity dropzone, DragEventPhase phase, TriggerCondition condition) {
	auto& d = dropzone.Get<impl::Dropzone>();

	switch (phase) {
		using enum ptgn::DragEventPhase;
		case MoveOver: d.move_condition = condition; break;
		case Drop:	   d.drop_condition = condition; break;
		case Pickup:   d.pickup_condition = condition; break;
		default:	   PTGN_ERROR("Unhandled DragEventPhase");
	}
}

const std::unordered_set<Entity>& GetDropzones(Entity draggable) {
	return draggable.Get<impl::Draggable>().dropzones;
}

[[nodiscard]] const std::unordered_set<Entity>& GetDraggables(Entity dropzone) {
	return dropzone.Get<impl::Dropzone>().draggables;
}

Entity SetInteractive(Entity entity, bool interactive) {
	entity.TryAdd<impl::Interactive>().enabled = interactive;
	return entity;
}

Entity RemoveInteractive(Entity entity) {
	entity.Remove<impl::Interactive>();
	return entity;
}

bool IsInteractive(Entity entity) {
	return entity.Has<impl::Interactive>() && entity.Get<impl::Interactive>().enabled;
}

Entity SetInteractable(
	Entity entity, GameObject&& shape, std::string_view name, bool ignore_parent_transform
) {
	ClearInteractables(entity);
	AddInteractable(entity, std::move(shape), name, ignore_parent_transform);
	return entity;
}

Entity AddInteractable(
	Entity entity, GameObject&& shape, std::string_view name, bool ignore_parent_transform
) {
	IgnoreParentTransform(shape, ignore_parent_transform);
	SetInteractive(entity);
	if (!name.empty()) {
		PTGN_ASSERT(
			!HasChild(entity, name),
			"Cannot add the same named interactable to an entity more than once"
		);
	}
	AddChild(entity, shape, name);
	auto& shapes{ impl::GetInteractive(entity).shapes };
	shapes.emplace_back(GameObject{ std::move(shape) });
	return entity;
}

Entity RemoveInteractable(Entity entity, std::string_view name) {
	if (!IsInteractive(entity)) {
		return entity;
	}
	if (!HasChild(entity, name)) {
		return entity;
	}
	Entity child{ GetChild(entity, name) };
	auto& shapes{ impl::GetInteractive(entity).shapes };
	std::erase(shapes, child);
	return entity;
}

bool HasInteractable(Entity entity, std::string_view name) {
	if (!IsInteractive(entity)) {
		return false;
	}
	if (!HasChild(entity, name)) {
		return false;
	}
	Entity child{ GetChild(entity, name) };
	const auto& shapes{ impl::GetInteractive(entity).shapes };

	for (const auto& shape : shapes) {
		if (shape == child) {
			return true;
		}
	}
	return false;
}

std::vector<Entity> GetInteractables(Entity entity) {
	PTGN_ASSERT(IsInteractive(entity));
	const auto& shapes{ impl::GetInteractive(entity).shapes };
	std::vector<Entity> interactables;
	interactables.reserve(shapes.size());
	for (const auto& shape : shapes) {
		interactables.emplace_back(shape);
	}
	return interactables;
}

void ClearShapes() {
	shapes.clear();
}

void ClearInteractables(Entity entity) {
	if (!entity.Has<Interactive>()) {
		return;
	}
	auto& interactive{ impl::GetInteractive(entity) };
	// Clear owned entities.
	interactive.ClearShapes();
}

Entity SetDraggable(Entity entity, bool draggable) {
	entity.TryAdd<impl::Draggable>().enabled = draggable;
	return entity;
}

bool IsDraggable(Entity entity) {
	return false;
}

namespace impl {

const Interactive& GetInteractive(Entity entity) {
	PTGN_ASSERT(IsInteractive(entity));
	return impl::EntityAccess::Get<Interactive>(entity);
}

Interactive& GetInteractive(Entity entity) {
	return const_cast<Interactive&>(GetInteractive(std::as_const(entity)));
}

} // namespace impl

} // namespace ptgn