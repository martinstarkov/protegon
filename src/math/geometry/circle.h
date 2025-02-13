#pragma once

#include <cstdint>
#include <vector>

#include "math/geometry/intersection.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "math/vector4.h"

namespace ptgn {

struct Color;
struct Rect;
struct Line;
struct Capsule;
struct RoundedRect;

struct Circle {
	V2_float center;
	float radius{ 0.0f };

	// center += offset
	void Offset(const V2_float& offset);

	[[nodiscard]] V2_float Center() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;

	[[nodiscard]] Intersection Intersects(const Circle& circle) const;
	[[nodiscard]] Intersection Intersects(const Rect& rect) const;

	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Line& line) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Circle& circle) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Capsule& capsule) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Rect& rect) const;
};

struct Arc {
	V2_float center;
	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };

private:
	friend struct RoundedRect;
	friend struct Capsule;

	// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
	// (false).
	// @param sa start_angle clamped between 0 and 2 pi.
	// @param ea end_angle clamped between 0 and 2 pi.
	// @return The vertices which make up the arc.
	[[nodiscard]] std::vector<V2_float> GetVertices(bool clockwise, float sa, float ea) const;
};

struct Ellipse {
	V2_float center;
	V2_float radius;
};

} // namespace ptgn