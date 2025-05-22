#include "components/transform.h"

#include "math/vector2.h"

namespace ptgn {

Transform::Transform(const V2_float& transform_position) : position{ transform_position } {}

Transform::Transform(
	const V2_float& transform_position, float transform_rotation, const V2_float& transform_scale
) :
	position{ transform_position }, rotation{ transform_rotation }, scale{ transform_scale } {}

Transform Transform::RelativeTo(Transform parent) const {
	parent.position += position;
	parent.rotation += rotation;
	parent.scale	*= scale;
	return parent;
}

} // namespace ptgn