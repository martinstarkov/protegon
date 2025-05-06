#include "components/transform.h"

#include "math/vector2.h"

namespace ptgn {

Transform::Transform(const V2_float& position) : position{ position } {}

Transform::Transform(const V2_float& position, float rotation, const V2_float& scale) :
	position{ position }, rotation{ rotation }, scale{ scale } {}

[[nodiscard]] Transform Transform::RelativeTo(Transform parent) const {
	parent.position += position;
	parent.rotation += rotation;
	parent.scale	*= scale;
	return parent;
}

} // namespace ptgn