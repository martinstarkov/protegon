#include "runtime/ecs/components/transform_component.h"

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/camera/camera.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"

namespace ptgn {

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

Transform GetDrawTransform(Entity entity) {
	auto offset_transform{ GetOffset(entity) };
	PTGN_ASSERT(!entity.Has<impl::Camera>());
	auto transform{ GetWorldTransform(entity) };
	transform = transform.RelativeTo(offset_transform);
	return transform;
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

void SetTransform(Entity entity, Transform transform) {
	entity.template Add<Transform>(transform);
}

void SetPosition(Entity entity, V2_float position) {
	auto transform{ GetTransform(entity) };
	transform.SetPosition(position);
	SetTransform(entity, transform);
}

void SetPositionX(Entity entity, float position_x) {
	SetPosition(entity, V2_float{ position_x, GetPosition(entity).y });
}

void SetPositionY(Entity entity, float position_y) {
	SetPosition(entity, V2_float{ GetPosition(entity).x, position_y });
}

void Translate(Entity entity, V2_float position_difference) {
	SetPosition(entity, GetPosition(entity) + position_difference);
}

void TranslateX(Entity entity, float position_x_difference) {
	Translate(entity, V2_float{ position_x_difference, 0.0f });
}

void TranslateY(Entity entity, float position_y_difference) {
	Translate(entity, V2_float{ 0.0f, position_y_difference });
}

void SetRotation(Entity entity, float rotation) {
	auto transform{ GetTransform(entity) };
	transform.SetRotation(rotation);
	SetTransform(entity, transform);
}

void Rotate(Entity entity, float angle_difference) {
	SetRotation(entity, GetRotation(entity) + angle_difference);
}

void SetScale(Entity entity, V2_float scale) {
	auto transform{ GetTransform(entity) };
	transform.SetScale(scale);
	SetTransform(entity, transform);
}

void SetScale(Entity entity, float scale) {
	SetScale(entity, V2_float{ scale });
}

void SetScaleX(Entity entity, float scale_x) {
	SetScale(entity, V2_float{ scale_x, GetScale(entity).y });
}

void SetScaleY(Entity entity, float scale_y) {
	SetScale(entity, V2_float{ GetScale(entity).x, scale_y });
}

void Scale(Entity entity, V2_float scale_multiplier) {
	SetScale(entity, GetScale(entity) * scale_multiplier);
}

void ScaleX(Entity entity, float scale_x_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.x *= scale_x_multiplier;
	SetScale(entity, scale);
}

void ScaleY(Entity entity, float scale_y_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.y *= scale_y_multiplier;
	SetScale(entity, scale);
}

} // namespace ptgn