#include "runtime/interaction/interactive.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
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
	Entity entity, GameObject<>&& shape, std::optional<std::string_view> shape_id,
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
	Entity entity, GameObject<>&& shape, std::optional<std::string_view> shape_id,
	bool ignore_parent_transform
) {
	ClearInteractiveShapes(entity);
	AddInteractiveShape(entity, std::move(shape), shape_id, ignore_parent_transform);
}

void AddInteractiveRect(
	Entity interactive_entity, V2_float position, V2_float size, Origin draw_origin,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	auto& scene{ interactive_entity.GetScene() };
	auto shape = scene.CreateEntity();
	shape.Add<Rect>(size);
	SetPosition(shape, position);
	SetDrawOrigin(shape, draw_origin);
	AddInteractiveShape(
		interactive_entity, GameObject{ std::move(shape) }, shape_id, ignore_parent_transform
	);
}

void SetInteractiveRect(
	Entity interactive_entity, V2_float position, V2_float size, Origin draw_origin,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	ClearInteractiveShapes(interactive_entity);
	AddInteractiveRect(
		interactive_entity, position, size, draw_origin, shape_id, ignore_parent_transform
	);
}

void AddInteractiveCircle(
	Entity interactive_entity, V2_float position, float radius,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	auto& scene{ interactive_entity.GetScene() };
	auto shape = scene.CreateEntity();
	shape.Add<Circle>(radius);
	SetPosition(shape, position);
	AddInteractiveShape(
		interactive_entity, GameObject{ std::move(shape) }, shape_id, ignore_parent_transform
	);
}

void SetInteractiveCircle(
	Entity interactive_entity, V2_float position, float radius,
	std::optional<std::string_view> shape_id, bool ignore_parent_transform
) {
	ClearInteractiveShapes(interactive_entity);
	AddInteractiveCircle(interactive_entity, position, radius, shape_id, ignore_parent_transform);
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

} // namespace ptgn