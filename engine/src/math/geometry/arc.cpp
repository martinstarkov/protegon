#include "math/geometry/arc.h"

#include <array>

#include "core/ecs/components/draw.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "debug/runtime/assert.h"
#include "math/math_utils.h"
#include "math/vector2.h"

namespace ptgn {

Arc::Arc(float arc_radius, float start_angle, float end_angle, bool clockwise) :
	radius{ arc_radius },
	start_angle{ start_angle },
	end_angle{ end_angle },
	clockwise{ clockwise } {}

void Arc::Draw(const Entity& entity) {
	impl::DrawArc(entity);
}

V2_float Arc::GetCenter(const Transform& transform) const {
	return transform.GetPosition();
}

float Arc::GetRadius() const {
	return radius;
}

float Arc::GetRadius(const Transform& transform) const {
	return GetRadius() * Abs(transform.GetAverageScale());
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

std::array<V2_float, 4> Arc::GetWorldQuadVertices(const Transform& transform) const {
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