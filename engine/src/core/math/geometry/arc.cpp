#include "core/math/geometry/arc.h"

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

Arc::Arc(float arc_radius, float start_angle, float end_angle, bool clockwise) :
	radius{ arc_radius },
	start_angle{ start_angle },
	end_angle{ end_angle },
	clockwise{ clockwise } {}

V2_float Arc::GetCenter(Transform transform) const {
	return transform.GetPosition();
}

float Arc::GetRadius() const {
	return radius;
}

float Arc::GetRadius(Transform transform) const {
	auto arc_radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return arc_radius * std::abs(scale);
}

float Arc::GetStartAngle() const {
	return start_angle;
}

float Arc::GetEndAngle() const {
	return end_angle;
}

float Arc::GetAperture() const {
	float aperture{ ClampAngle2Pi(end_angle - start_angle + two_pi<float>) };
	return aperture;
}

std::array<V2_float, 4> Arc::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Arc::GetLocalQuadVertices() const {
	V2_float min{ -radius };
	V2_float max{ radius };
	PTGN_ASSERT(min != max, "Cannot get local vertices for an arc with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

} // namespace ptgn