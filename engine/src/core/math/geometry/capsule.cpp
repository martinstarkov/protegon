#include "math/geometry/capsule.h"

#include <array>

#include "ecs/components/transform.h"
#include "math/geometry/rect.h"
#include "math/math_utils.h"
#include "math/vector2.h"

namespace ptgn {

Capsule::Capsule(V2_float start, V2_float end, float radius) :
	start{ start }, end{ end }, radius{ radius } {}

std::array<V2_float, 4> Capsule::GetWorldQuadVertices(
	const Transform& transform, V2_float* out_size
) const {
	auto dir{ end - start };

	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	auto local_center{ start + dir * 0.5f };

	V2_float center{ transform.Apply(local_center) };

	float rotation{ dir.Angle() };

	auto diameter{ 2.0f * GetRadius() };

	Rect rect{ V2_float{ diameter + dir.Magnitude(), diameter } };
	if (out_size) {
		*out_size = rect.GetSize(transform);
	}
	return rect.GetWorldVertices(Transform{ center, rotation, transform.GetScale() });
}

std::array<V2_float, 2> Capsule::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 2> Capsule::GetLocalVertices() const {
	return { start, end };
}

float Capsule::GetRadius() const {
	return radius;
}

float Capsule::GetRadius(const Transform& transform) const {
	return GetRadius() * Abs(transform.GetAverageScale());
}

} // namespace ptgn