#pragma once

#include <array>
#include <vector>

#include "collision/raycast.h"
#include "math/geometry/axis.h"
#include "math/geometry/intersection.h"
#include "math/vector2.h"
#include "renderer/layer_info.h"
#include "renderer/origin.h"

namespace ptgn {

struct Color;
struct Line;
struct Circle;
struct Capsule;
struct Rect;

struct Triangle {
	Triangle() = default;
	Triangle(const V2_float& a, const V2_float& b, const V2_float& c);

	[[nodiscard]] bool operator==(const Triangle& o) const {
		return a == o.a && b == o.b && c == o.c;
	}

	[[nodiscard]] bool operator!=(const Triangle& o) const {
		return !(*this == o);
	}

	V2_float a;
	V2_float b;
	V2_float c;

	// @return True if the point is inside the triangle.
	[[nodiscard]] bool Overlaps(const V2_float& point) const;

	[[nodiscard]] bool Overlaps(const Rect& rect) const;

	// @return True if internal triangle is entirely contained by the triangle.
	[[nodiscard]] bool Contains(const Triangle& internal) const;

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;
};

struct Rect {
	V2_float position;
	V2_float size;
	Origin origin{ Origin::Center };
	// Rotation in radians relative to the center of the rectangle.
	float rotation{ 0.0f };

	Rect() = default;

	Rect(
		const V2_float& position, const V2_float& size = {}, Origin origin = Origin::Center,
		float rotation = 0.0f
	);

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;

	// position += offset
	void Offset(const V2_float& offset);

	// @return Each of the line segments which make up the rectangle. Currently only works for
	// unrotated rectangles. Order is clockwise.
	[[nodiscard]] std::array<Line, 4> GetWalls() const;

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
	[[nodiscard]] bool Overlaps(const Triangle& triangle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;

	[[nodiscard]] Intersection Intersects(const Rect& rect) const;
	[[nodiscard]] Intersection Intersects(const Circle& circle) const;

	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Circle& circle) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Rect& rect) const;

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

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;

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

	// @return True if triangle is entirely contained by the polygon.
	[[nodiscard]] bool Contains(const Triangle& triangle) const;

	// Only works if both polygons are convex.
	[[nodiscard]] Intersection Intersects(const Polygon& polygon) const;

private:
	[[nodiscard]] bool GetMinimumOverlap(const Polygon& polygon, float& depth, Axis& axis) const;
	[[nodiscard]] bool HasOverlapAxis(const Polygon& polygon) const;
};

} // namespace ptgn