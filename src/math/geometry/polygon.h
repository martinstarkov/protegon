#pragma once

#include <array>
#include <vector>

#include "math/geometry/axis.h"
#include "math/geometry/intersection.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Line;
struct Circle;

struct Rect {
	V2_float position;
	V2_float size;
	Origin origin{ Origin::Center };
	// Rotation in radians relative to the center of the rectangle.
	float rotation{ 0.0f };

	Rect() = default;

	Rect(
		const V2_float& position, const V2_float& size, Origin origin = Origin::Center,
		float rotation = 0.0f
	);

	// @return Half the size of the rectangle.
	[[nodiscard]] V2_float Half() const;

	// @return Center position of rectangle.
	[[nodiscard]] V2_float Center() const;

	// @return Bottom right position of the unrotated rectangle.
	[[nodiscard]] V2_float Max() const;

	// @return Top left position of the unrotated rectangle.
	[[nodiscard]] V2_float Min() const;

	// @param rotation_center {0, 0} is top left, { 0.5, 0.5 } is center, { 1, 1 } is bottom right.
	[[nodiscard]] std::array<V2_float, 4> GetVertices(
		const V2_float& rotation_center = { 0.5f, 0.5f }
	) const;

	[[nodiscard]] bool IsZero() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& o_rect) const;

	[[nodiscard]] Intersection Intersects(const Rect& o_rect) const;
	[[nodiscard]] Intersection Intersects(const Circle& circle) const;

private:
	static void OffsetVertices(
		std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin
	);

	// Rotation angle in radians.
	static void RotateVertices(
		std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
		float rotation_radians, const V2_float& rotation_center
	);
};

struct Polygon {
	std::vector<V2_float> vertices;

	Polygon() = default;

	// Rotation in radians.
	explicit Polygon(const Rect& rect);

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