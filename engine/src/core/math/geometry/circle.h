#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Circle {
	Circle() = default;

	Circle(float radius); // NOSONAR

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	/// @return { radius * 2, radius * 2 }
	V2_float GetSize() const;

	/// @return { radius * 2, radius * 2 } scaled relative to the transform.
	V2_float GetSize(Transform transform) const;

	float GetRadius() const;

	/// @return Radius scaled relative to the transform.
	float GetRadius(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Circle&) const = default;

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Circle, radius)
};

} // namespace ptgn