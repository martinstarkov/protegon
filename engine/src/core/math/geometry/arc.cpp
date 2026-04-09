#include "core/math/geometry/arc.h"

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

void Arc::SetRadius(float radius) {
	radius_ = radius;
}

void Arc::SetStartAngle(Radians start_angle) {
	start_angle_ = start_angle;
}

void Arc::SetEndAngle(Radians end_angle) {
	end_angle_ = end_angle;
}

void Arc::SetStartAngle(Degrees start_angle) {
	start_angle_ = start_angle.ToRad();
}

void Arc::SetEndAngle(Degrees end_angle) {
	end_angle_ = end_angle.ToRad();
}

void Arc::SetClockwise(bool clockwise) {
	clockwise_ = clockwise;
}

V2_float Arc::GetCenter(Transform transform) const {
	return transform.GetPosition();
}

float Arc::GetRadius() const {
	return radius_;
}

float Arc::GetRadius(Transform transform) const {
	auto arc_radius{ GetRadius() };
	auto scale{ transform.GetAverageScale() };
	return arc_radius * std::abs(scale);
}

Degrees Arc::GetStartAngle() const {
	return start_angle_.ToDeg();
}

Degrees Arc::GetEndAngle() const {
	return end_angle_.ToDeg();
}

Degrees Arc::GetAperture() const {
	auto start = start_angle_;
	auto end   = end_angle_;

	Radians delta;

	if (clockwise_) {
		delta = end - start;
	} else {
		delta = start - end;
	}

	delta = Clamp(delta);

	// Handle full circle edge case
	if (NearlyEqual(delta.value, 0.0f) && start != end) {
		return Radians{ kTwoPi }.ToDeg();
	}

	return delta.ToDeg();
}

bool Arc::IsClockwise() const {
	return clockwise_;
}

std::array<V2_float, 4> Arc::GetWorldQuadVertices(Transform transform) const {
	auto local_vertices{ GetLocalQuadVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 4> Arc::GetLocalQuadVertices() const {
	V2_float min{ -radius_ };
	V2_float max{ radius_ };
	PTGN_ASSERT(min != max, "Cannot get local vertices for an arc with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

} // namespace ptgn