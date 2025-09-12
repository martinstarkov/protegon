#pragma once

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

} // namespace impl

struct Circle {
	Circle() = default;

	Circle(float radius);

	static void Draw(const Entity& entity);

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	[[nodiscard]] float GetRadius() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] float GetRadius(const Transform& transform) const;

	// @return min, max coordinates that contain the circle.
	[[nodiscard]] std::array<V2_float, 2> GetExtents(const Transform& transform) const;

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Circle, radius)
};

PTGN_DRAWABLE_REGISTER(Circle);

} // namespace ptgn