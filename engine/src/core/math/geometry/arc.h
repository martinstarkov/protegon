#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Arc {
	Arc() = default;

	/// @brief Unit: Radians
	Arc(float arc_radius, float start_angle, float end_angle, bool clockwise = true);

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	float GetRadius() const;
	float GetStartAngle() const;
	float GetEndAngle() const;
	float GetAperture() const;

	/// @return Radius scaled relative to the transform.
	float GetRadius(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Arc&) const = default;

	float radius{ 0.0f };
	/// @brief Unit: Radians
	float start_angle{ 0.0f };
	/// @brief Unit: Radians
	float end_angle{ 0.0f };

	/// @brief Direction of arc.
	bool clockwise{ true };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Arc, radius, start_angle, end_angle, clockwise)
};

} // namespace ptgn