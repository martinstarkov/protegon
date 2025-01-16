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

struct LayerInfo;
struct Color;
struct Line;
struct Circle;
struct Capsule;
struct Rect;
struct Polygon;
struct RoundedRect;

namespace impl {

class RenderData;

// @param vertex_modulo The modulo applied to the i + 1th vertex. If 0, use the value of
// vertex_count. Non-zero value used by Arc, which draws vertices.size() - 1 vertices, but requires
// modulo to be vertices.size().
void DrawVertices(
	const V2_float* vertices, std::size_t vertex_count, float line_width, const V4_float& color,
	std::int32_t render_layer, RenderData& render_data, std::size_t vertex_modulo = 0
);

} // namespace impl

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

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;

private:
	friend struct Polygon;

	void DrawSolid(const V4_float& color, std::int32_t render_layer, impl::RenderData& render_data)
		const;

	void DrawThick(
		float line_width, const V4_float& color, std::int32_t render_layer,
		impl::RenderData& render_data
	) const;
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

	// Uses default render target.
	virtual void Draw(const Color& color, float line_width = -1.0f) const;

	virtual void Draw(
		const Color& color, float line_width, const LayerInfo& layer_info,
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
		const V4_float& color, const std::array<V2_float, 4>& vertices, std::int32_t render_layer,
		impl::RenderData& render_data
	);

	static void DrawThick(
		float line_width, const V4_float& color, const std::array<V2_float, 4>& vertices,
		std::int32_t render_layer, impl::RenderData& render_data
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

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const override;

	void Draw(
		const Color& color, float line_width, const LayerInfo& layer_info,
		const V2_float& rotation_center = { 0.5f, 0.5f }
	) const override;
};

struct Polygon {
	std::vector<V2_float> vertices;

	Polygon() = default;

	// Rotation in radians.
	explicit Polygon(const Rect& rect);

	explicit Polygon(const std::vector<V2_float>& vertices);

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;

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
	void DrawSolid(const V4_float& color, std::int32_t render_layer, impl::RenderData& render_data)
		const;

	void DrawThick(
		float line_width, const V4_float& color, std::int32_t render_layer,
		impl::RenderData& render_data
	) const;

	[[nodiscard]] bool GetMinimumOverlap(const Polygon& polygon, float& depth, Axis& axis) const;

	[[nodiscard]] bool HasOverlapAxis(const Polygon& polygon) const;
};

} // namespace ptgn