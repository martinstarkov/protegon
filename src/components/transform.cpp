#include "components/transform.h"

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

} // namespace ptgn