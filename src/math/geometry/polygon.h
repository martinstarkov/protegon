#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "math/geometry/axis.h"
#include "math/geometry/intersection.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/origin.h"

namespace ptgn {

struct Color;
struct Line;
struct Circle;
struct Capsule;
struct Rect;
struct Polygon;
struct RoundedRect;

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

	// @param line_width -1 for a solid fille triangle.
	void Draw(const Color& color, float line_width = -1.0f, std::int32_t render_layer = 0) const;

private:
	friend struct Polygon;

	void DrawSolid(const V4_float& color, std::int32_t render_layer) const;

	void DrawThick(float line_width, const V4_float& color, std::int32_t render_layer) const;
};

struct Rect {
	V2_float position;
	V2_float size;
	Origin origin{ Origin::Center };
	// Rotation in radians relative to the center of the rectangle.
	float rotation{ 0.0f };

	Rect()			= default;
	virtual ~Rect() = default;

	Rect(
		const V2_float& position, const V2_float& size = {}, Origin origin = Origin::Center,
		float rotation = 0.0f
	);

	[[nodiscard]] static Rect Fullscreen();

	// @param line_width -1 for a solid filled rectangle.
	virtual void Draw(
		const Color& color, float line_width = -1.0f, std::int32_t render_layer = 0,
		const V2_float& rotation_center = { 0.5f, 0.5f }
	) const;

	[[nodiscard]] bool operator==(const Rect& o) const {
		return position == o.position && size == o.size && origin == o.origin &&
			   rotation == o.rotation;
	}

	[[nodiscard]] bool operator!=(const Rect& o) const {
		return !(*this == o);
	}

	// position += offset
	void Offset(const V2_float& offset);

	// @return Each of the line segments which make up the rectangle. Currently only works for
	// unrotated rectangles. Order is clockwise starting from top edge.
	[[nodiscard]] std::array<Line, 4> GetEdges() const;

	// @return Each of the corners which make up the rectangle. Currently only works for
	// unrotated rectangles. Order is clockwise starting from top left point..
	[[nodiscard]] std::array<V2_float, 4> GetCorners() const;

	// @return Half the size of the rectangle.
	[[nodiscard]] V2_float Half() const;

	// @return Center position of rectangle.
	[[nodiscard]] V2_float Center() const;

	// @return Bottom right position of the unrotated rectangle.
	[[nodiscard]] V2_float Max() const;

	// @return Top left position of the unrotated rectangle.
	[[nodiscard]] V2_float Min() const;

	// @return Position of the unrotated rectangle relative to the given origin.
	[[nodiscard]] V2_float GetPosition(Origin relative_to) const;

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
	friend struct RoundedRect;

	static void OffsetVertices(
		std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin
	);

	// Rotation angle in radians.
	static void RotateVertices(
		std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
		float rotation_radians, const V2_float& rotation_center
	);

	static void DrawSolid(
		const V4_float& color, const std::array<V2_float, 4>& vertices, std::int32_t render_layer
	);

	static void DrawThick(
		float line_width, const V4_float& color, const std::array<V2_float, 4>& vertices,
		std::int32_t render_layer
	);
};

struct RoundedRect : public Rect {
	float radius{ 0.0f };

	RoundedRect() = default;

	explicit RoundedRect(
		const V2_float& position, float radius = 0.0f, const V2_float& size = {},
		Origin origin = Origin::Center, float rotation = 0.0f
	);

	// @return The inner rectangle which consists of the the vertices connecting the corner arc
	// start points, i.e. the asterisk * region in the following illustration.
	//  /-------\
	//  |*******|
	//  |*******|
	//  \-------/
	[[nodiscard]] Rect GetInnerRect() const;

	// @return The outer rectangle which contains the entirely of the rounded rectangle as if it had
	// no radius.
	[[nodiscard]] Rect GetOuterRect() const;

	// @param line_width -1 for a solid filled rounded rectangle.
	void Draw(
		const Color& color, float line_width = -1.0f, std::int32_t render_layer = 0,
		const V2_float& rotation_center = { 0.5f, 0.5f }
	) const override;
};

struct Polygon {
	std::vector<V2_float> vertices;

	Polygon() = default;

	// Rotation in radians.
	explicit Polygon(const Rect& rect);

	explicit Polygon(const std::vector<V2_float>& vertices);

	// @param line_width -1 for a solid filled polygon.
	void Draw(const Color& color, float line_width = -1.0f, std::int32_t render_layer = 0) const;

	// @return Centroid of the polygon.
	[[nodiscard]] V2_float Center() const;

	// @return True if all the interior angles are less than 180 degrees.
	[[nodiscard]] bool IsConvex() const;

	// @return True if any of the interior angles are above 180 degrees.
	[[nodiscard]] bool IsConcave() const;

	// @return A vector of triangles which make up the polygon.
	[[nodiscard]] std::vector<Triangle> Triangulate() const;

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
	void DrawSolid(const V4_float& color, std::int32_t render_layer) const;

	void DrawThick(float line_width, const V4_float& color, std::int32_t render_layer) const;

	[[nodiscard]] bool GetMinimumOverlap(const Polygon& polygon, float& depth, Axis& axis) const;

	[[nodiscard]] bool HasOverlapAxis(const Polygon& polygon) const;
};

namespace impl {

// @param vertex_modulo The modulo applied to the i + 1th vertex. If 0, use the value of
// vertex_count. Non-zero value used by Arc, which draws vertices.size() - 1 vertices, but requires
// modulo to be vertices.size().
void DrawVertices(
	const V2_float* vertices, std::size_t vertex_count, float line_width, const V4_float& color,
	std::int32_t render_layer, std::size_t vertex_modulo = 0
);

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<Triangle> Triangulate(const V2_float* contour, std::size_t count);

} // namespace impl

} // namespace ptgn