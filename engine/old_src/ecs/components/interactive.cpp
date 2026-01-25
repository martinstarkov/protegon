#include "ecs/components/interactive.h"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ecs/entity.h"
#include "ecs/entity_hierarchy.h"
#include "ecs/game_object.h"
#include "math/vector2.h"

namespace ptgn {

V2_float Draggable::GetOffset() const {
	return offset_;
}

V2_float Draggable::GetStart() const {
	return start_;
}

const std::unordered_set<Entity>& Draggable::GetDropzones() const {
	return dropzones_;
}

bool Draggable::IsBeingDragged() const {
	return dragging_;
}

void Dropzone::SetTrigger(CallbackTrigger trigger) {
	move_trigger_	= trigger;
	drop_trigger_	= trigger;
	pickup_trigger_ = trigger;
}

void Dropzone::SetMoveTrigger(CallbackTrigger trigger) {
	move_trigger_ = trigger;
}

void Dropzone::SetDropTrigger(CallbackTrigger trigger) {
	drop_trigger_ = trigger;
}

void Dropzone::SetPickupTrigger(CallbackTrigger trigger) {
	pickup_trigger_ = trigger;
}

[[nodiscard]] const std::unordered_set<Entity>& Dropzone::GetDroppedEntities() const {
	return dropped_entities_;
}

Entity& SetInteractive(Entity& entity, bool interactive) {
	impl::EntityAccess::TryAdd<Interactive>(entity).enabled = interactive;
	return entity;
}

Entity& RemoveInteractive(Entity& entity) {
	impl::EntityAccess::Remove<Interactive>(entity);
	return entity;
}

bool IsInteractive(const Entity& entity) {
	return entity.Has<Interactive>() && entity.Get<Interactive>().enabled;
}

Entity& SetInteractable(
	Entity& entity, Entity&& shape, std::string_view name, bool ignore_parent_transform
) {
	ClearInteractables(entity);
	AddInteractable(entity, std::move(shape), name, ignore_parent_transform);
	return entity;
}

Entity& AddInteractable(
	Entity& entity, Entity&& shape, std::string_view name, bool ignore_parent_transform
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

Entity& RemoveInteractable(Entity& entity, std::string_view name) {
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

bool HasInteractable(const Entity& entity, std::string_view name) {
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

std::vector<Entity> GetInteractables(const Entity& entity) {
	PTGN_ASSERT(IsInteractive(entity));
	const auto& shapes{ impl::GetInteractive(entity).shapes };
	std::vector<Entity> interactables;
	interactables.reserve(shapes.size());
	for (const auto& shape : shapes) {
		interactables.emplace_back(shape);
	}
	return interactables;
}

void Interactive::ClearShapes() {
	shapes.clear();
}

void ClearInteractables(Entity& entity) {
	if (!entity.Has<Interactive>()) {
		return;
	}
	auto& interactive{ impl::GetInteractive(entity) };
	// Clear owned entities.
	interactive.ClearShapes();
}

namespace impl {

const Interactive& GetInteractive(const Entity& entity) {
	PTGN_ASSERT(IsInteractive(entity));
	return impl::EntityAccess::Get<Interactive>(entity);
}

Interactive& GetInteractive(Entity& entity) {
	return const_cast<Interactive&>(GetInteractive(std::as_const(entity)));
}

} // namespace impl

} // namespace ptgn