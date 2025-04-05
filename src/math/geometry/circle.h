#pragma once

#include <vector>

#include "core/transform.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

struct Circle {
	Circle() = default;
	explicit Circle(float radius);

	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER(Circle, radius)
};

struct Arc {
	Arc() = default;

	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };

	PTGN_SERIALIZER_REGISTER(Arc, radius, start_angle, end_angle)
private:
	// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
	// (false).
	// @param start_angle Must be in range: [0, 2pi).
	// @param end_angle Must be in range: [0, 2pi).
	// @return The vertices which make up the arc.
	[[nodiscard]] std::vector<V2_float> GetVertices(
		const V2_float& center, bool clockwise, float start_angle, float end_angle
	) const;
};

struct Ellipse {
	Ellipse() = default;
	explicit Ellipse(const V2_float& radius);

	V2_float radius;

	PTGN_SERIALIZER_REGISTER(Ellipse, radius)
};

[[nodiscard]] V2_float GetCenter(const Transform& transform, const Circle& circle);

} // namespace ptgn