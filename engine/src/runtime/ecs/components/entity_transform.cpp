#include "runtime/ecs/components/entity_transform.h"

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

Entity SetTransform(Entity entity, Transform transform) {
	entity.template Add<Transform>(transform);
	return entity;
}

Transform GetTransform(Entity entity) {
	return entity.template TryAdd<Transform>();
}

Transform GetWorldTransform(Entity entity) {
	auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>()) {
		return transform;
	}
	Transform relative_to;
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		relative_to = GetWorldTransform(parent);
	}
	auto world_transform{ transform.RelativeTo(relative_to) };
	return world_transform;
}

V2_float GetPosition(Entity entity) {
	return GetTransform(entity).GetPosition();
}

V2_float GetWorldPosition(Entity entity) {
	return GetWorldTransform(entity).GetPosition();
}

float GetRotation(Entity entity) {
	return GetTransform(entity).GetRotation();
}

float GetWorldRotation(Entity entity) {
	return GetWorldTransform(entity).GetRotation();
}

V2_float GetScale(Entity entity) {
	return GetTransform(entity).GetScale();
}

V2_float GetWorldScale(Entity entity) {
	return GetWorldTransform(entity).GetScale();
}

Entity SetPosition(Entity entity, V2_float position) {
	auto transform{ GetTransform(entity) };
	transform.SetPosition(position);
	return SetTransform(entity, transform);
}

Entity SetPositionX(Entity entity, float position_x) {
	return SetPosition(entity, V2_float{ position_x, GetPosition(entity).y });
}

Entity SetPositionY(Entity entity, float position_y) {
	return SetPosition(entity, V2_float{ GetPosition(entity).x, position_y });
}

Entity Translate(Entity entity, V2_float position_difference) {
	return SetPosition(entity, GetPosition(entity) + position_difference);
}

Entity TranslateX(Entity entity, float position_x_difference) {
	return Translate(entity, V2_float{ position_x_difference, 0.0f });
}

Entity TranslateY(Entity entity, float position_y_difference) {
	return Translate(entity, V2_float{ 0.0f, position_y_difference });
}

Entity SetRotation(Entity entity, float rotation) {
	auto transform{ GetTransform(entity) };
	transform.SetRotation(rotation);
	return SetTransform(entity, transform);
}

Entity Rotate(Entity entity, float angle_difference) {
	return SetRotation(entity, GetRotation(entity) + angle_difference);
}

Entity SetScale(Entity entity, V2_float scale) {
	auto transform{ GetTransform(entity) };
	transform.SetScale(scale);
	return SetTransform(entity, transform);
}

Entity SetScale(Entity entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

Entity SetScaleX(Entity entity, float scale_x) {
	return SetScale(entity, V2_float{ scale_x, GetScale(entity).y });
}

Entity SetScaleY(Entity entity, float scale_y) {
	return SetScale(entity, V2_float{ GetScale(entity).x, scale_y });
}

Entity Scale(Entity entity, V2_float scale_multiplier) {
	return SetScale(entity, GetScale(entity) * scale_multiplier);
}

Entity ScaleX(Entity entity, float scale_x_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.x *= scale_x_multiplier;
	return SetScale(entity, scale);
}

Entity ScaleY(Entity entity, float scale_y_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.y *= scale_y_multiplier;
	return SetScale(entity, scale);
}

} // namespace ptgn