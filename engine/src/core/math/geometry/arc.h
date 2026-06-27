#pragma once

#include <array>
#include <cstdlib>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Arc {
public:
	float radius{ 0.0f };
	Radians start_angle{ 0.0f };
	Radians end_angle{ 0.0f };

	/// @brief Direction of arc.
	bool clockwise{ true };

	constexpr Arc() = default;

	constexpr Arc(float arc_radius, Radians start_angle, Radians end_angle, bool clockwise = true) :
		radius{ arc_radius },
		start_angle{ start_angle },
		end_angle{ end_angle },
		clockwise{ clockwise } {}

	constexpr Arc(float arc_radius, Degrees start_angle, Degrees end_angle, bool clockwise = true) :
		Arc{ arc_radius, start_angle.ToRad(), end_angle.ToRad(), clockwise } {}

	constexpr Degrees GetAperture() const {
		auto start = start_angle;
		auto end   = end_angle;

		Radians delta;

		if (clockwise) {
			delta = start - end;
		} else {
			delta = end - start;
		}

		delta = Clamp(delta);

		// Handle full circle edge case
		if (NearlyEqual(delta.value, 0.0f) && start != end) {
			return Radians{ kTwoPi }.ToDeg();
		}

		return delta.ToDeg();
	}

	/// @return Radius scaled relative to the transform.
	constexpr float GetRadius(Transform transform) const {
		auto avg_scale{ transform.GetAverageScale() };
		auto abs_scale{ std::abs(avg_scale) };
		return radius * abs_scale;
	}

	constexpr std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const {
		auto vertices{ GetLocalQuadVertices() };
		return transform.Apply(vertices);
	}

	constexpr std::array<V2_float, 4> GetLocalQuadVertices() const {
		V2_float min{ -radius };
		V2_float max{ radius };
		return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
	}

	constexpr bool operator==(const Arc&) const = default;

	PTGN_SERIALIZE(Arc, radius, start_angle, end_angle, clockwise)
};

} // namespace ptgn