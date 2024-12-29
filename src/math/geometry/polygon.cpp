#include "math/geometry/polygon.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "collision/raycast.h"
#include "core/game.h"
#include "math/geometry/axis.h"
#include "math/geometry/circle.h"
#include "math/geometry/intersection.h"
#include "math/geometry/line.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"

namespace ptgn {

bool Triangle::Overlaps(const V2_float& point) const {
	// Using barycentric coordinates method.
	float A{ 0.5f * (-b.y * c.x + a.y * (-b.x + c.x) + a.x * (b.y - c.y) + b.x * c.y) };
	float z{ 1.0f / (2.0f * A) };
	float s{ z * (a.y * c.x - a.x * c.y + (c.y - a.y) * point.x + (a.x - c.x) * point.y) };
	float t{ z * (a.x * b.y - a.y * b.x + (a.y - b.y) * point.x + (b.x - a.x) * point.y) };

	return s >= 0.0f && t >= 0.0f && (s + t) <= 1.0f;
}

bool Triangle::Overlaps(const Rect& rect) const {
	V2_float min{ rect.Min() };
	V2_float max{ rect.Max() };
	return Polygon{ { a, b, c } }.Overlaps(Polygon{
		{ min, { max.x, min.y }, max, { min.x, max.y } } });
}

bool Triangle::Contains(const Triangle& internal) const {
	return Overlaps(internal.a) && Overlaps(internal.b) && Overlaps(internal.c);
}

Rect::Rect(const V2_float& position, const V2_float& size, Origin origin, float rotation) :
	position{ position }, size{ size }, origin{ origin }, rotation{ rotation } {}

void Rect::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.draw.Rect(*this, color, line_width, { 0.5f, 0.5f }, layer_info);
}

void Rect::Offset(const V2_float& offset) {
	position += offset;
}

std::array<Line, 4> Rect::GetWalls() const {
	V2_int min{ Min() };
	V2_int max{ Max() };
	return { Line{ min, { max.x, min.y } }, Line{ { max.x, min.y }, max },
			 Line{ max, { min.x, max.y } }, Line{ { min.x, max.y }, min } };
}

V2_float Rect::Half() const {
	return size * 0.5f;
}

// @return Center position of rectangle.
V2_float Rect::Center() const {
	return position - GetOffsetFromCenter(size, origin);
}

// @return Bottom right position of rectangle.
V2_float Rect::Max() const {
	return Center() + Half();
}

// @return Top left position of rectangle.
V2_float Rect::Min() const {
	return Center() - Half();
}

void Rect::OffsetVertices(
	std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin
) {
	auto draw_offset = GetOffsetFromCenter(size, draw_origin);

	// Offset each vertex by based on draw origin.
	if (!draw_offset.IsZero()) {
		for (auto& v : vertices) {
			v -= draw_offset;
		}
	}
}

void Rect::RotateVertices(
	std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
	float rotation_radians, const V2_float& rotation_center
) {
	PTGN_ASSERT(
		rotation_center.x >= 0.0f && rotation_center.x <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);
	PTGN_ASSERT(
		rotation_center.y >= 0.0f && rotation_center.y <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);

	V2_float half{ size * 0.5f };

	V2_float rot{ -size * rotation_center };

	V2_float s0{ rot };
	V2_float s1{ size.x + rot.x, rot.y };
	V2_float s2{ size + rot };
	V2_float s3{ rot.x, size.y + rot.y };

	float c{ 1.0f };
	float s{ 0.0f };

	if (!NearlyEqual(rotation_radians, 0.0f)) {
		c = std::cos(rotation_radians);
		s = std::sin(rotation_radians);
	}

	auto rotated = [&](const V2_float& coordinate) {
		return position - rot - half +
			   V2_float{ c * coordinate.x - s * coordinate.y, s * coordinate.x + c * coordinate.y };
	};

	vertices[0] = rotated(s0);
	vertices[1] = rotated(s1);
	vertices[2] = rotated(s2);
	vertices[3] = rotated(s3);
}

std::array<V2_float, 4> Rect::GetVertices(const V2_float& rotation_center) const {
	std::array<V2_float, 4> vertices;
	RotateVertices(vertices, position, size, rotation, rotation_center);
	OffsetVertices(vertices, size, origin);
	return vertices;
}

bool Rect::IsZero() const {
	return position.IsZero() && size.IsZero();
}

bool Rect::Overlaps(const V2_float& point) const {
	if (rotation != 0.0f) {
		Polygon poly_a{ *this };
		return poly_a.Overlaps(point);
	}

	V2_float max{ Max() };
	V2_float min{ Min() };

	if (point.x < min.x || point.x > max.x) {
		return false;
	}

	if (point.y < min.y || point.y > max.y) {
		return false;
	}

	if (NearlyEqual(point.x, max.x) || NearlyEqual(point.x, min.x)) {
		return false;
	}

	if (NearlyEqual(point.y, min.y) || NearlyEqual(point.y, max.y)) {
		return false;
	}

	return true;
}

bool Rect::Overlaps(const Line& line) const {
	// TODO: Add rotation check.

	V2_float c{ Center() };
	V2_float e{ Half() };
	V2_float m{ line.Midpoint() };
	V2_float d{ line.b - m }; // Line halflength vector

	m = m - c;				  // Translate box and segment to origin

	// Try world coordinate axes as separating axes.
	float adx{ FastAbs(d.x) };
	if (FastAbs(m.x) >= e.x + adx) {
		return false;
	}
	float ady{ FastAbs(d.y) };
	if (FastAbs(m.y) >= e.y + ady) {
		return false;
	}
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis.
	adx += epsilon<float>;
	ady += epsilon<float>;

	// Try cross products of segment direction vector with coordinate axes.
	float cross{ m.Cross(d) };
	float dot{ e.Dot({ ady, adx }) };

	if (FastAbs(cross) > dot) {
		return false;
	}
	// No separating axis found; segment must be overlapping AABB.
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool Rect::Overlaps(const Triangle& triangle) const {
	// TODO: Add rotation check.
	return triangle.Overlaps(*this);
}

bool Rect::Overlaps(const Circle& circle) const {
	// TODO: Add rotation check.
	return circle.Overlaps(*this);
}

bool Rect::Overlaps(const Capsule& capsule) const {
	// TODO: Add rotation check.
	return capsule.Overlaps(*this);
}

bool Rect::Overlaps(const Rect& o_rect) const {
	if (rotation != 0.0f || o_rect.rotation != 0.0f) {
		Polygon poly_a{ *this };
		Polygon poly_b{ o_rect };

		return poly_a.Overlaps(poly_b);
	}

	V2_float max{ Max() };
	V2_float min{ Min() };
	V2_float o_max{ o_rect.Max() };
	V2_float o_min{ o_rect.Min() };

	if (max.x < o_min.x || min.x > o_max.x) {
		return false;
	}

	if (max.y < o_min.y || min.y > o_max.y) {
		return false;
	}

	// Optional: Ignore seam collisions:

	if (NearlyEqual(min.x, o_max.x) || NearlyEqual(max.x, o_min.x)) {
		return false;
	}

	if (NearlyEqual(max.y, o_min.y) || NearlyEqual(min.y, o_max.y)) {
		return false;
	}

	return true;
}

Intersection Rect::Intersects(const Rect& o_rect) const {
	if (rotation != 0.0f || o_rect.rotation != 0.0f) {
		Polygon poly_a{ *this };
		Polygon poly_b{ o_rect };

		return poly_a.Intersects(poly_b);
	}

	Intersection c;

	V2_float a_h{ Half() };
	V2_float b_h{ o_rect.Half() };
	V2_float d{ o_rect.Center() - Center() };
	V2_float pen{ a_h + b_h - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

	// Optional: To include seams in collision, simply remove the NearlyEqual calls from this if
	// statement.
	if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f)) {
		return c;
	}

	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		// Edge case where aabb centers are in the same location.
		c.normal.y = -1.0f; // upward
		c.depth	   = a_h.y + b_h.y;
	} else if (pen.y < pen.x) {
		c.normal.y = -Sign(d.y);
		c.depth	   = FastAbs(pen.y);
	} else {
		c.normal.x = -Sign(d.x);
		c.depth	   = FastAbs(pen.x);
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection Rect::Intersects(const Circle& circle) const {
	Intersection c{ circle.Intersects(*this) };
	c.normal *= -1.0f;
	return c;
}

ptgn::Raycast Rect::Raycast(const V2_float& ray, const Circle& circle) const {
	return circle.Raycast(-ray, *this);
}

ptgn::Raycast Rect::Raycast(const V2_float& ray, const Rect& rect) const {
	V2_float a_center{ Center() };
	Line line{ a_center, a_center + ray };
	Rect expanded{ rect.Min() - Half(), rect.size + size, Origin::TopLeft };
	auto raycast{ line.Raycast(expanded) };
	return raycast;
}

Polygon::Polygon(const Rect& rect) {
	float c_a{ std::cos(rect.rotation) };
	float s_a{ std::sin(rect.rotation) };

	const auto rotated = [c_a, s_a](const V2_float& v) {
		return V2_float{ v.x * c_a - v.y * s_a, v.x * s_a + v.y * c_a };
	};

	vertices.resize(4);

	V2_float center{ rect.Center() };

	V2_float min{ rect.Min() - center };
	V2_float max{ rect.Max() - center };

	vertices[0] = rotated({ min.x, max.y }) + center;
	vertices[1] = rotated(max) + center;
	vertices[2] = rotated({ max.x, min.y }) + center;
	vertices[3] = rotated(min) + center;
}

Polygon::Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {}

void Polygon::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.draw.Polygon(vertices.data(), vertices.size(), color, line_width, layer_info);
}

V2_float Polygon::Center() const {
	// Source: https://stackoverflow.com/a/63901131
	V2_float centroid;
	float signed_area{ 0.0f };
	V2_float v0{ 0.0f }; // Current verte
	V2_float v1{ 0.0f }; // Next vertex
	float a{ 0.0f };	 // Partial signed area

	std::size_t lastdex	 = vertices.size() - 1;
	const V2_float* prev = &(vertices[lastdex]);
	const V2_float* next{ nullptr };

	// For all vertices in a loop
	for (const auto& vertex : vertices) {
		next		 = &vertex;
		v0			 = *prev;
		v1			 = *next;
		a			 = v0.Cross(v1);
		signed_area += a;
		centroid	+= (v0 + v1) * a;
		prev		 = next;
	}

	signed_area *= 0.5f;
	centroid	/= 6.0f * signed_area;

	return centroid;
}

bool Polygon::IsConvex() const {
	PTGN_ASSERT(vertices.size() >= 3, "Line or point convexity check is redundant");

	const auto get_cross = [](const V2_float& a, const V2_float& b, const V2_float& c) {
		return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
	};

	int sign{ static_cast<int>(Sign(get_cross(vertices[0], vertices[1], vertices[2]))) };

	// For convex polygons, all sequential point triplet cross products must have the same sign (+
	// or -). For convex polygon every triplet makes turn in the same side (or CW, or CCW depending
	// on walk direction). For concave one some signs will differ (where inner angle exceeds 180
	// degrees). Note that you don't need to calculate angle values. Source:
	// https://stackoverflow.com/a/40739079

	// Skip first point since that is the established reference.
	for (std::size_t i = 1; i < vertices.size(); i++) {
		V2_float a{ vertices[(i + 0)] };
		V2_float b{ vertices[(i + 1) % vertices.size()] };
		V2_float c{ vertices[(i + 2) % vertices.size()] };

		int new_sign{ static_cast<int>(Sign(get_cross(a, b, c))) };

		if (new_sign != sign) {
			// Polygon is concave.
			return false;
		}
	}

	// Convex.
	return true;
}

bool Polygon::IsConcave() const {
	return !IsConvex();
}

bool Polygon::HasOverlapAxis(const Polygon& polygon) const {
	const auto axes{ impl::GetAxes(*this, false) };
	for (Axis a : axes) {
		auto [min1, max1] = impl::GetProjectionMinMax(*this, a);
		auto [min2, max2] = impl::GetProjectionMinMax(polygon, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
	}
	return true;
}

bool Polygon::Overlaps(const Polygon& polygon) const {
	PTGN_ASSERT(
		IsConvex() && polygon.IsConvex(),
		"PolygonPolygon overlap check only works if both polygons are convex"
	);
	return HasOverlapAxis(polygon) && polygon.HasOverlapAxis(*this);
}

bool Polygon::Overlaps(const V2_float& point) const {
	const auto& v{ vertices };
	std::size_t count{ v.size() };
	bool c{ false };
	std::size_t i{ 0 };
	std::size_t j{ count - 1 };
	// Algorithm from: https://wrfranklin.org/Research/Short_Notes/pnpoly.html
	for (; i < count; j = i++) {
		bool a{ (v[i].y > point.y) != (v[j].y > point.y) };
		bool b{ point.x < (v[j].x - v[i].x) * (point.y - v[i].y) / (v[j].y - v[i].y) + v[i].x };
		if (a && b) {
			c = !c;
		}
	}
	return c;
}

bool Polygon::Contains(const Triangle& triangle) const {
	return Overlaps(triangle.a) && Overlaps(triangle.b) && Overlaps(triangle.c);
}

bool Polygon::Contains(const Polygon& internal) const {
	for (const auto& p : internal.vertices) {
		if (!Overlaps(p)) {
			return false;
		}
	}
	return true;
}

bool Polygon::GetMinimumOverlap(const Polygon& polygon, float& depth, Axis& axis) const {
	const auto axes{ impl::GetAxes(*this, true) };
	for (Axis a : axes) {
		auto [min1, max1] = impl::GetProjectionMinMax(*this, a);
		auto [min2, max2] = impl::GetProjectionMinMax(polygon, a);

		if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
			return false;
		}
		bool contained{ Contains(polygon) || polygon.Contains(*this) };

		float o{ impl::GetIntervalOverlap(min1, max1, min2, max2, contained, axis.direction) };

		if (o < depth) {
			depth = o;
			axis  = a;
		}
	}
	return true;
};

Intersection Polygon::Intersects(const Polygon& polygon) const {
	PTGN_ASSERT(
		IsConvex() && polygon.IsConvex(),
		"PolygonPolygon intersection check only works if both polygons are convex"
	);

	Intersection c;

	float depth{ std::numeric_limits<float>::infinity() };
	Axis axis;

	if (!GetMinimumOverlap(polygon, depth, axis) ||
		!polygon.GetMinimumOverlap(*this, depth, axis)) {
		return c;
	}

	PTGN_ASSERT(depth != std::numeric_limits<float>::infinity());
	PTGN_ASSERT(depth >= 0.0f);

	// Make sure the vector is pointing from polygon1 to polygon2.
	if (V2_float dir{ Center() - polygon.Center() }; dir.Dot(axis.direction) < 0) {
		axis.direction *= -1.0f;
	}

	c.normal = axis.direction;
	c.depth	 = depth;

	return c;

	/*
	// Useful debug drawing code:
	// Draw all polygon points projected onto all the axes.
	const auto draw_axes = [](const std::vector<Axis>& axes, const Polygon& p) {
		for (const auto& a : axes) {
			game.draw.Axis(a.midpoint, a.direction, color::Pink, 1.0f);
			auto [min, max] = impl::GetProjectionMinMax(p, a);
			V2_float p1{ a.midpoint + a.direction * min };
			V2_float p2{ a.midpoint + a.direction * max };
			game.draw.Point(p1, color::Purple, 5.0f);
			game.draw.Point(p2, color::Orange, 5.0f);
			V2_float to{ 0.0f, -17.0f };
			game.draw.Text(std::to_string((int)min), p1 + to, color::Purple);
			game.draw.Text(std::to_string((int)max), p2 + to, color::Orange);
		}
	};

	draw_axes(impl::GetAxes(*this, true), *this);
	draw_axes(impl::GetAxes(*this, true), polygon);
	draw_axes(impl::GetAxes(polygon, true), *this);
	draw_axes(impl::GetAxes(polygon, true), polygon);

	// Draw overlap axis and overlap amounts on both sides.
	game.draw.Axis(axis.midpoint, c.normal, color::Black, 2.0f);
	game.draw.Line(axis.midpoint, axis.midpoint - c.normal * c.depth, color::Cyan, 3.0f);
	game.draw.Line(axis.midpoint, axis.midpoint + c.normal * c.depth, color::Cyan, 3.0f);
	*/
}

Triangle::Triangle(const V2_float& a, const V2_float& b, const V2_float& c) :
	a{ a }, b{ b }, c{ c } {}

void Triangle::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.draw.Triangle(a, b, c, color, line_width, layer_info);
}

} // namespace ptgn
