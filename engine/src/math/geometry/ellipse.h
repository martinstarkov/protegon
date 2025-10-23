#pragma once

#include <array>

#include "core/ecs/components/drawable.h"
#include "core/utils/concepts.h"
#include "math/vector2.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

} // namespace impl

struct Ellipse {
	Ellipse() = default;

	template <Arithmetic T>
	explicit Ellipse(const Vector2<T>& ellipse_radius) : radius{ ellipse_radius } {}

	static void Draw(const Entity& entity);

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	[[nodiscard]] V2_float GetRadius() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] V2_float GetRadius(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldQuadVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Ellipse&) const = default;

	V2_float radius;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Ellipse, radius)
};

PTGN_DRAWABLE_REGISTER(Ellipse);

} // namespace ptgn