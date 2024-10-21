#pragma once

#include "math/geometry/intersection.h"
#include "math/vector2.h"

namespace ptgn {

struct Rect;
struct Line;

struct Circle {
	V2_float center;
	float radius{ 0.0f };

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;

	[[nodiscard]] Intersection Intersects(const Circle& circle) const;
	[[nodiscard]] Intersection Intersects(const Rect& rect) const;

private:
	[[nodiscard]] static bool WithinPerimeter(float radius, float dist2);
};

struct Arc {
	V2_float center;
	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };
};

struct Ellipse {
	V2_float center;
	V2_float radius;
};

} // namespace ptgn