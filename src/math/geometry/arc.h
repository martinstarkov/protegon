#pragma once

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

struct ReverseArc {};

} // namespace impl

struct Arc {
	Arc() = default;

	Arc(float arc_radius, float start_angle, float end_angle);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	[[nodiscard]] float GetRadius() const;
	[[nodiscard]] float GetStartAngle() const;
	[[nodiscard]] float GetEndAngle() const;
	[[nodiscard]] float GetAperture() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] float GetRadius(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetLocalQuadVertices() const;

	float radius{ 0.0f };
	float start_angle{ 0.0f };
	float end_angle{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Arc, radius, start_angle, end_angle)
};

void SetArcReversed(Entity& entity, bool reversed = true);

PTGN_DRAWABLE_REGISTER(Arc);

} // namespace ptgn