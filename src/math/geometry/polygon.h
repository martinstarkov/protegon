#pragma once

#include <vector>

#include "math/geometry/axis.h"
#include "math/geometry/intersection.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Line;
struct Circle;

// Rectangles are axis aligned bounding boxes (AABBs).
struct Rect {
	V2_float pos;
	V2_float size;
	Origin origin{ Origin::Center };

	Rect() = default;

	Rect(const V2_float& pos, const V2_float& size, Origin origin = Origin::Center);

	// @return Half the size of the rectangle.
	[[nodiscard]] V2_float Half() const;

	// @return Center position of rectangle.
	[[nodiscard]] V2_float Center() const;

	// @return Bottom right position of rectangle.
	[[nodiscard]] V2_float Max() const;

	// @return Top left position of rectangle.
	[[nodiscard]] V2_float Min() const;

	[[nodiscard]] Rect Offset(const V2_float& pos_amount, const V2_float& size_amount = {}) const;

	[[nodiscard]] Rect Scale(const V2_float& scale) const;

	[[nodiscard]] bool IsZero() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	// Optional: Rotations in radians, around the center of the rectangles.
	[[nodiscard]] bool Overlaps(const Rect& o_rect, float rotation = 0.0f, float o_rotation = 0.0f)
		const;

	// Optional: Rotations in radians, around the center of the rectangles.
	[[nodiscard]] Intersection Intersects(
		const Rect& o_rect, float rotation = 0.0f, float o_rotation = 0.0f
	) const;
	[[nodiscard]] Intersection Intersects(const Circle& circle) const;
};

struct Polygon {
	std::vector<V2_float> vertices;

	Polygon() = default;

	// Rotation in radians.
	Polygon(const Rect& rect, float rotation);

	explicit Polygon(const std::vector<V2_float>& vertices);

	// @return Centroid of the polygon.
	[[nodiscard]] V2_float Center() const;

	// @return True if all the interior angles are less than 180 degrees.
	[[nodiscard]] bool IsConvex() const;
	// @return True if any of the interior angles are above 180 degrees.
	[[nodiscard]] bool IsConcave() const;

	// Only works if both polygons are convex.
	[[nodiscard]] bool Overlaps(const Polygon& polygon) const;

	// Works for both convex and concave polygons.
	[[nodiscard]] bool Overlaps(const V2_float& point) const;

	// @return True if internal polygon is entirely contained by this polygon.
	[[nodiscard]] bool Contains(const Polygon& internal) const;

	// Only works if both polygons are convex.
	[[nodiscard]] Intersection Intersects(const Polygon& polygon) const;

private:
	[[nodiscard]] bool GetMinimumOverlap(const Polygon& polygon, float& depth, Axis& axis) const;
	[[nodiscard]] bool HasOverlapAxis(const Polygon& polygon) const;
};

struct Triangle {
	Triangle() = default;
	Triangle(const V2_float& a, const V2_float& b, const V2_float& c);
	V2_float a;
	V2_float b;
	V2_float c;
};

} // namespace ptgn