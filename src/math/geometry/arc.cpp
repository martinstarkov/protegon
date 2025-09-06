#include "math/geometry/arc.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Arc::Arc(float arc_radius, float start_angle, float end_angle) :
	radius{ arc_radius }, start_angle{ start_angle }, end_angle{ end_angle } {}

void Arc::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawArc(ctx, entity, entity.Has<impl::ReverseArc>());
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
	return ApplyTransform(local_vertices, transform);
}

std::array<V2_float, 4> Arc::GetLocalQuadVertices() const {
	V2_float min{ -radius };
	V2_float max{ radius };
	PTGN_ASSERT(min != max, "Cannot get local vertices for an arc with size zero");
	return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
}

void SetArcReversed(Entity& entity, bool reversed) {
	if (reversed) {
		entity.TryAdd<impl::ReverseArc>();
	} else {
		entity.Remove<impl::ReverseArc>();
	}
}

} // namespace ptgn