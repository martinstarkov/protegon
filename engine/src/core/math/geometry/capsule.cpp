#include "core/math/geometry/capsule.h"

#include <array>
#include <cstdlib>

#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

void Capsule::SetStart(V2_float start) {
	start_ = start;
}

void Capsule::SetEnd(V2_float end) {
	end_ = end;
}

void Capsule::SetRadius(float radius) {
	radius_ = radius;
}

V2_float Capsule::GetStart() const {
	return start_;
}

V2_float Capsule::GetEnd() const {
	return end_;
}

V2_float Capsule::GetDirection() const {
	return end_ - start_;
}

std::array<V2_float, 4> Capsule::GetWorldQuadVertices(Transform transform, V2_float* out_size)
	const {
	auto dir{ GetDirection() };

	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	auto local_center{ start_ + dir * 0.5f };

	V2_float center{ transform.Apply(local_center) };

	auto rotation{ dir.Angle() };

	auto diameter{ 2.0f * GetRadius() };

	Rect rect{ V2_float{ diameter + dir.Magnitude(), diameter } };

	if (out_size) {
		*out_size = rect.GetSize(transform);
	}

	Transform rect_transform{ center, rotation, transform.GetScale() };

	return rect.GetWorldVertices(rect_transform);
}

std::array<V2_float, 2> Capsule::GetWorldVertices(Transform transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 2> Capsule::GetLocalVertices() const {
	return { start_, end_ };
}

float Capsule::GetRadius() const {
	return radius_;
}

float Capsule::GetRadius(Transform transform) const {
	auto capsule_radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return capsule_radius * std::abs(scale);
}

} // namespace ptgn