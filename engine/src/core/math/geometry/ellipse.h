#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Ellipse {
	Ellipse() = default;

	template <Arithmetic T>
	explicit Ellipse(Vector2<T> ellipse_radius) : radius{ ellipse_radius } {}

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	V2_float GetRadius() const;

	/// @return Radius scaled relative to the transform.
	V2_float GetRadius(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Ellipse&) const = default;

	V2_float radius;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Ellipse, radius)
};

} // namespace ptgn