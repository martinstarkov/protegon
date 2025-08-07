#include "components/transform.h"

#include "components/offsets.h"
#include "core/entity.h"
#include "math/vector2.h"

namespace ptgn {

Transform::Transform(const V2_float& transform_position) : position{ transform_position } {}

Transform::Transform(
	const V2_float& transform_position, float transform_rotation, const V2_float& transform_scale
) :
	position{ transform_position }, rotation{ transform_rotation }, scale{ transform_scale } {}

Transform Transform::RelativeTo(const Transform& parent) const {
	Transform result;
	result.scale	= parent.scale * scale;
	result.rotation = parent.rotation + rotation;
	result.position = parent.position + (parent.scale * position).Rotated(parent.rotation);
	return result;
}

Transform Transform::InverseRelativeTo(const Transform& parent) const {
	Transform local;

	// Inverse scale and rotation
	float inv_rotation = -parent.rotation;
	V2_float inv_scale = { parent.scale.x != 0 ? 1.0f / parent.scale.x : 0.0f,
						   parent.scale.y != 0 ? 1.0f / parent.scale.y : 0.0f };

	// Compute delta position
	V2_float delta = position - parent.position;

	// Unrotate and unscale the position
	local.position	= delta.Rotated(inv_rotation);
	local.position *= inv_scale;

	// Rotation and scale
	local.rotation = rotation - parent.rotation;
	local.scale	   = scale * inv_scale;

	return local;
}

float Transform::GetAverageScale() const {
	return (scale.x + scale.y) * 0.5f;
}

Entity& SetTransform(Entity& entity, const Transform& transform) {
	if (entity.Has<Transform>()) {
		entity.GetImpl<Transform>() = transform;
	} else {
		entity.Add<Transform>(transform);
	}
	return entity;
}

Transform& GetTransform(Entity& entity) {
	return entity.TryAdd<Transform>();
}

Transform GetTransform(const Entity& entity) {
	if (auto transform{ entity.TryGet<Transform>() }; transform) {
		return *transform;
	}
	return {};
}

Transform GetAbsoluteTransform(const Entity& entity) {
	auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>() && entity.Get<impl::IgnoreParentTransform>()) {
		return transform;
	}
	Transform relative_to;
	if (HasParent(entity)) {
		auto parent{ GetParent(entity) };
		relative_to = GetAbsoluteTransform(parent);
	}
	return transform.RelativeTo(relative_to);
}

Transform GetDrawTransform(const Entity& entity) {
	auto offset_transform{ GetOffset(entity) };
	auto transform{ GetAbsoluteTransform(entity) };
	transform = transform.RelativeTo(offset_transform);
	return transform;
}

Entity& SetPosition(Entity& entity, const V2_float& position) {
	GetTransform(entity).position = position;
	return entity;
}

V2_float GetPosition(const Entity& entity) {
	return GetTransform(entity).position;
}

V2_float& GetPosition(Entity& entity) {
	return GetTransform(entity).position;
}

V2_float GetAbsolutePosition(const Entity& entity) {
	return GetAbsoluteTransform(entity).position;
}

Entity& SetRotation(Entity& entity, float rotation) {
	GetTransform(entity).rotation = rotation;
	return entity;
}

float GetRotation(const Entity& entity) {
	return GetTransform(entity).rotation;
}

float& GetRotation(Entity& entity) {
	return GetTransform(entity).rotation;
}

float GetAbsoluteRotation(const Entity& entity) {
	return GetAbsoluteTransform(entity).rotation;
}

Entity& SetScale(Entity& entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

Entity& SetScale(Entity& entity, const V2_float& scale) {
	GetTransform(entity).scale = scale;
	return entity;
}

V2_float GetScale(const Entity& entity) {
	return GetTransform(entity).scale;
}

V2_float& GetScale(Entity& entity) {
	return GetTransform(entity).scale;
}

V2_float GetAbsoluteScale(const Entity& entity) {
	return GetAbsoluteTransform(entity).scale;
}

} // namespace ptgn