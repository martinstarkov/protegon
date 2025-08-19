#include "components/interactive.h"

#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "math/vector2.h"
#include "utility/span.h"

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
	if (interactive) {
		impl::EntityAccess::Add<Interactive>(entity);
	} else {
		impl::ClearInteractables(entity);
		impl::EntityAccess::Remove<Interactive>(entity);
	}
	return entity;
}

bool IsInteractive(const Entity& entity) {
	return entity.Has<Interactive>();
}

Entity& SetInteractable(Entity& entity, Entity& shape, bool set_parent) {
	impl::ClearInteractables(entity);
	AddInteractable(entity, shape, set_parent);
	return entity;
}

Entity& AddInteractable(Entity& entity, Entity& shape, bool set_parent) {
	if (set_parent) {
		SetParent(shape, entity);
	}
	SetInteractive(entity);
	auto& shapes{ impl::GetInteractive(entity).shapes };
	PTGN_ASSERT(
		!VectorContains(shapes, shape),
		"Cannot add the same interactable to an entity more than once"
	);
	shapes.emplace_back(shape);
	return entity;
}

Entity& RemoveInteractable(Entity& entity, const Entity& shape) {
	if (!IsInteractive(entity)) {
		return entity;
	}
	auto& shapes{ impl::GetInteractive(entity).shapes };
	VectorErase(shapes, shape);
	return entity;
}

bool HasInteractable(const Entity& entity, const Entity& shape) {
	if (!IsInteractive(entity)) {
		return false;
	}
	const auto& shapes{ impl::GetInteractive(entity).shapes };
	return VectorContains(shapes, shape);
}

const std::vector<Entity>& GetInteractables(const Entity& entity) {
	PTGN_ASSERT(IsInteractive(entity));
	const auto& shapes{ impl::GetInteractive(entity).shapes };
	return shapes;
}

void Interactive::ClearShapes() {
	// TODO: Move to using GameObjects.
	for (Entity shape : shapes) {
		shape.Destroy();
	}
	shapes.clear();
}

namespace impl {

const Interactive& GetInteractive(const Entity& entity) {
	PTGN_ASSERT(IsInteractive(entity));
	return impl::EntityAccess::Get<Interactive>(entity);
}

Interactive& GetInteractive(Entity& entity) {
	return const_cast<Interactive&>(GetInteractive(std::as_const(entity)));
}

void ClearInteractables(Entity& entity) {
	if (!entity.Has<Interactive>()) {
		return;
	}
	auto& interactive{ GetInteractive(entity) };
	// Clear owned entities.
	interactive.ClearShapes();
}

} // namespace impl

} // namespace ptgn