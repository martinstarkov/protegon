#pragma once

#include <array>

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

	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Circle&) const = default;

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Circle, radius)
};

PTGN_DRAWABLE_REGISTER(Circle);

} // namespace ptgn