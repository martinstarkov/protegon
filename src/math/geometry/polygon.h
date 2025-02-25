#pragma once

#include <array>
#include <vector>

#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/intersect.h"
#include "math/collision/raycast.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

namespace impl {

// Rotation angle in radians.
[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& rect_center, float rotation, const V2_float& rect_size,
	const V2_float& rotation_center
);

// Rotation angle in radians.
[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& rect_min, const V2_float& rect_max, float rotation,
	const V2_float& rotation_center
);

[[nodiscard]] V2_float GetPolygonCenter(const std::vector<V2_float>& polygon);

} // namespace impl

struct Color;
struct Line;
struct Circle;
struct Capsule;
struct Rect;
struct Polygon;

struct Triangle : public GameObject {
	Triangle() = default;
	Triangle(const ecs::Entity& e);
	Triangle(
		const ecs::Entity& e, const V2_float& local_a, const V2_float& local_b,
		const V2_float& local_c
	);

	Triangle& SetLocalVertices(const V2_float& a, const V2_float& b, const V2_float& c);

	[[nodiscard]] std::array<V2_float, 3> GetVertices() const;

	bool operator==(const Triangle& o) const;

	bool operator!=(const Triangle& o) const;

	// @return True if the point is inside the triangle.
	[[nodiscard]] bool Overlaps(const V2_float& point) const;

	[[nodiscard]] bool Overlaps(const Rect& rect) const;

	// @return True if internal triangle is entirely contained by the triangle.
	[[nodiscard]] bool Contains(const Triangle& internal) const;

private:
	// Local vertex coordinates of the triangle.
	V2_float a_;
	V2_float b_;
	V2_float c_;
};

struct Rect : public GameObject {
	Rect() = default;
	Rect(const ecs::Entity& e);
	Rect(const ecs::Entity& e, const V2_float& size, Origin origin = Origin::Center);

	// @return { min, max }
	[[nodiscard]] std::array<V2_float, 2> GetExtents() const;

	Rect& SetSize(const V2_float& size);
	Rect& SetOrigin(Origin origin);

	[[nodiscard]] V2_float GetSize() const;
	[[nodiscard]] Origin GetOrigin() const;

	// @return Half the size of the rectangle.
	[[nodiscard]] V2_float Half() const noexcept;

	// @return Center position of rectangle.
	[[nodiscard]] V2_float GetCenter() const noexcept;

	// @return Bottom right position of the unrotated rectangle.
	[[nodiscard]] V2_float Max() const noexcept;

	// @return Top left position of the unrotated rectangle.
	[[nodiscard]] V2_float Min() const noexcept;

	// @return Position of the unrotated rectangle relative to the given origin.
	[[nodiscard]] V2_float GetPositionRelativeTo(Origin relative_to) const;

	[[nodiscard]] std::array<V2_float, 4> GetVertices() const;

	[[nodiscard]] bool IsZero() const noexcept;

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
	V2_float size_;
	Origin origin_{ Origin::Center };
};

/*
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
};
*/

struct Polygon : public GameObject {
	Polygon() = default;
	Polygon(const ecs::Entity& e);
	Polygon(const ecs::Entity& e, const std::vector<V2_float>& local_vertices);

	Polygon& SetLocalVertices(const std::vector<V2_float>& local_vertices);

	[[nodiscard]] const std::vector<V2_float>& GetLocalVertices() const;

	[[nodiscard]] std::vector<V2_float> GetVertices() const;

	// @return Centroid of the polygon.
	[[nodiscard]] V2_float GetCenter() const;

	// @return True if all the interior angles are less than 180 degrees.
	[[nodiscard]] bool IsConvex() const;

	// @return True if any of the interior angles are above 180 degrees.
	[[nodiscard]] bool IsConcave() const;

	// @return A vector of triangles which make up the polygon.
	[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate() const;

	// Works for both convex and concave polygons.
	[[nodiscard]] bool Overlaps(const V2_float& point) const;

	// Only works if both polygons are convex.
	[[nodiscard]] bool Overlaps(const Polygon& polygon) const;

	// @return True if internal polygon is entirely contained by this polygon.
	[[nodiscard]] bool Contains(const Polygon& internal) const;

	// @return True if triangle is entirely contained by the polygon.
	[[nodiscard]] bool Contains(const Triangle& triangle) const;

	// Only works if both polygons are convex.
	[[nodiscard]] Intersection Intersects(const Polygon& polygon) const;

private:
	// Vertex positions relative to the polygon transform position.
	std::vector<V2_float> local_vertices_;
};

namespace impl {

[[nodiscard]] float TriangulateArea(const V2_float* contour, std::size_t count);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
);

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate(
	const V2_float* contour, std::size_t count
);

} // namespace impl

} // namespace ptgn