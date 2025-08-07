#include "components/interactive.h"

#include <vector>

#include "core/entity.h"
#include "utility/span.h"

namespace ptgn {

Entity& SetInteractive(Entity& entity, bool interactive) {
	if (interactive) {
		entity.Add<Interactive>();
	} else {
		impl::ClearInteractables(entity);
		entity.Remove<Interactive>();
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

void Interactive::Clear() {
	for (Entity shape : shapes) {
		shape.Destroy();
	}
	shapes.clear();
}

namespace impl {

const Interactive& GetInteractive(const Entity& entity) {
	PTGN_ASSERT(IsInteractive(entity));
	return entity.GetImpl<Interactive>();
}

Interactive& GetInteractive(Entity& entity) {
	return const_cast<Interactive&>(GetInteractive(std::as_const(entity)));
}

void SetInteractiveWasInside(Entity& entity, bool value) {
	auto& interactive{ GetInteractive(entity) };
	interactive.was_inside = value;
}

void SetInteractiveIsInside(Entity& entity, bool value) {
	auto& interactive{ GetInteractive(entity) };
	interactive.is_inside = value;
}

bool InteractiveWasInside(const Entity& entity) {
	const auto& interactive{ GetInteractive(entity) };
	return interactive.was_inside;
}

bool InteractiveIsInside(const Entity& entity) {
	const auto& interactive{ GetInteractive(entity) };
	return interactive.is_inside;
}

void ClearInteractables(Entity& entity) {
	if (!entity.Has<Interactive>()) {
		return;
	}
	auto& interactive{ GetInteractive(entity) };
	// Clear owned entities.
	interactive.Clear();
}

} // namespace impl

} // namespace ptgn