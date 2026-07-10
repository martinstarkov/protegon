#include "runtime/interaction/interactive.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/scene/scene.h"

namespace ptgn {

void SetInteractive(Entity entity, ComponentState state) {
	impl::SetComponentState<impl::Interactive>(entity, state);
}

bool IsInteractive(Entity entity) {
	return entity.Has<impl::Interactive>() && entity.Get<impl::Interactive>().enabled;
}

void AddInteractiveShape(
	Entity entity, Entity shape, std::optional<std::string_view> shape_id,
	bool ignore_parent_transform
) {
	IgnoreParentTransform(shape, ignore_parent_transform);
	SetInteractive(entity);
	if (shape_id.has_value()) {
		PTGN_ASSERT(
			!HasChild(entity, shape_id.value()),
			"Cannot add the same named interactable to an entity more than once"
		);
	}
	shape.Add<impl::InteractiveTag>();
	AddChild(entity, shape, shape_id);
}

void SetInteractiveShape(
	Entity entity, Entity shape, std::optional<std::string_view> shape_id,
	bool ignore_parent_transform
) {
	ClearInteractiveShapes(entity);
	AddInteractiveShape(entity, shape, shape_id, ignore_parent_transform);
}

void AddInteractiveRect(
	Entity interactive_entity, Transform transform, V2_float size, Origin origin,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	auto& scene{ interactive_entity.GetScene() };
	auto shape = scene.CreateEntity();

	PTGN_DEFAULT_NAME(shape, "Interactive Rect");

	shape.Add<Rect>(size);
	shape.Add<Transform>(transform);
	shape.Add<Origin>(origin);

	AddInteractiveShape(interactive_entity, shape, shape_id, ignore_parent_transform);
}

void SetInteractiveRect(
	Entity interactive_entity, Transform transform, V2_float size, Origin origin,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	ClearInteractiveShapes(interactive_entity);
	AddInteractiveRect(
		interactive_entity, transform, size, origin, shape_id, ignore_parent_transform
	);
}

void AddInteractiveCircle(
	Entity interactive_entity, Transform transform, float radius,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	auto& scene{ interactive_entity.GetScene() };
	auto shape = scene.CreateEntity();
	PTGN_DEFAULT_NAME(shape, "Interactive Circle");
	shape.Add<Circle>(radius);
	SetTransform(shape, transform);
	AddInteractiveShape(interactive_entity, shape, shape_id, ignore_parent_transform);
}

void SetInteractiveCircle(
	Entity interactive_entity, Transform transform, float radius,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	ClearInteractiveShapes(interactive_entity);
	AddInteractiveCircle(interactive_entity, transform, radius, shape_id, ignore_parent_transform);
}

void RemoveInteractiveShape(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Interactive>()) {
		return;
	}
	if (!HasChild(entity, name)) {
		return;
	}
	Entity child{ GetChild(entity, name) };
	PTGN_ASSERT(
		child.Has<impl::InteractiveTag>(), "Cannot remove a child entity that is not interactive"
	);
	RemoveChild(entity, name);
	child.Destroy();
}

bool HasInteractiveShape(Entity entity) {
	if (!entity.Has<impl::Interactive>()) {
		return false;
	}
	if (entity.HasAny<Rect, Circle>()) {
		return true;
	}
	if (!HasChildren(entity)) {
		return false;
	}
	const auto& children{ GetChildren(entity) };
	return std::ranges::any_of(children, [](auto child) {
		return child.template Has<impl::InteractiveTag>();
	});
}

bool HasInteractiveShape(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Interactive>()) {
		return false;
	}
	if (!HasChild(entity, name)) {
		return false;
	}

	Entity child{ GetChild(entity, name) };

	return child.Has<impl::InteractiveTag>();
}

std::vector<Entity> GetInteractiveShapes(Entity entity) {
	PTGN_ASSERT(entity.Has<impl::Interactive>(), "Entity must have interactive component");
	if (!HasChildren(entity)) {
		return {};
	}
	const auto& children{ GetChildren(entity) };
	return std::ranges::to<std::vector<Entity>>(
		children |
		std::views::filter([](auto child) { return child.template Has<impl::InteractiveTag>(); })
	);
}

void ClearInteractiveShapes(Entity entity) {
	if (!entity.Has<impl::Interactive>()) {
		return;
	}
	if (!HasChildren(entity)) {
		return;
	}
	std::vector<Entity> children{ GetChildren(entity) };
	std::ranges::for_each(children, [entity](auto child) {
		if (child.template Has<impl::InteractiveTag>()) {
			RemoveChild(entity, child);
			child.Destroy();
		}
	});
}

} // namespace ptgn