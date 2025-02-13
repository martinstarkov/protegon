#include "math/geometry/circle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "math/geometry/intersection.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/raycast.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/assert.h"

namespace ptgn {

void Circle::Offset(const V2_float& offset) {
	center += offset;
}

V2_float Circle::Center() const {
	return center;
}

bool Circle::Overlaps(const V2_float& point) const {
	V2_float dist{ center - point };
	return impl::WithinPerimeter(radius, dist.Dot(dist));
}

bool Circle::Overlaps(const Circle& circle) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 88.
	V2_float dist{ center - circle.center };
	return impl::WithinPerimeter(radius + circle.radius, dist.Dot(dist));
}

bool Circle::Overlaps(const Rect& rect) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	return impl::WithinPerimeter(radius, impl::SquareDistancePointRect(center, rect));
}

bool Circle::Overlaps(const Capsule& capsule) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114.
	// If (squared) distance smaller than (squared) sum of radii, they collide
	return impl::WithinPerimeter(
		radius + capsule.radius, impl::SquareDistancePointLine(capsule.line, center)
	);
}

bool Circle::Overlaps(const Line& line) const {
	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection

	// If the line is inside the circle entirely, exit early.
	if (Overlaps(line.a) && Overlaps(line.b)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };

	// O is the circle center, P is the line origin, Q is the line destination.
	V2_float OP{ line.a - center };
	V2_float OQ{ line.b - center };
	V2_float PQ{ line.Direction() };

	float OP_dist2{ OP.Dot(OP) };
	float OQ_dist2{ OQ.Dot(OQ) };
	float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		float triangle_area{ FastAbs(impl::ParallelogramArea(center, line.a, line.b)) / 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}

	return impl::WithinPerimeter(radius, min_dist2) && !impl::WithinPerimeter(radius, max_dist2);
}

Intersection Circle::Intersects(const Circle& circle) const {
	Intersection c;

	V2_float d{ circle.center - center };
	float dist2{ d.Dot(d) };
	float r{ radius + circle.radius };

	// No overlap.
	if (!impl::WithinPerimeter(r, dist2)) {
		return c;
	}

	if (dist2 > epsilon2<float>) {
		float dist{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(dist, 0.0f));
		c.normal = -d / dist;
		c.depth	 = r - dist;
	} else {
		// Edge case where circle centers are in the same location.
		c.normal.y = -1.0f; // default to upward normal.
		c.depth	   = r;
	}

	c.depth = std::max(c.depth, 0.0f);

	return c;
}

Intersection Circle::Intersects(const Rect& rect) const {
	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	Intersection c;

	V2_float half{ rect.Half() };
	V2_float max{ rect.Max() };
	V2_float min{ rect.Min() };
	V2_float clamped{ std::clamp(center.x, min.x, max.x), std::clamp(center.y, min.y, max.y) };
	V2_float ab{ center - clamped };

	float dist2{ ab.Dot(ab) };

	// No overlap.
	if (!impl::WithinPerimeter(radius, dist2)) {
		return c;
	}

	if (!NearlyEqual(dist2, 0.0f)) {
		// Shallow intersection (center of circle not inside of AABB).
		float d{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(d, 0.0f));
		c.normal = ab / d;
		c.depth	 = radius - d;
		c.depth	 = std::max(c.depth, 0.0f);
		return c;
	}

	// Deep intersection (center of circle inside of AABB).

	// Clamp circle's center to edge of AABB, then form the manifold.
	V2_float mid{ rect.Center() };
	V2_float d{ mid - center };

	V2_float overlap{ half - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

	if (overlap.x < overlap.y) {
		c.depth	   = radius + overlap.x;
		c.normal.x = d.x < 0 ? 1.0f : -1.0f;
	} else {
		c.depth	   = radius + overlap.y;
		c.normal.y = d.y < 0 ? 1.0f : -1.0f;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Line& o_line) const {
	Line line{ center, center + ray };
	return line.Raycast(Capsule{ o_line.a, o_line.b, radius });
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Circle& circle) const {
	Line line{ center, center + ray };
	return line.Raycast(Circle{ circle.center, circle.radius + radius });
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Capsule& capsule) const {
	Line line{ center, center + ray };
	return line.Raycast(Capsule{ capsule.line, radius + capsule.radius });
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Rect& rect) const {
	// TODO: Fix corner collisions.
	// TODO: Consider
	// https://www.geometrictools.com/Documentation/IntersectionMovingCircleRectangle.pdf
	/*return Rect{ center, { 2.0f * radius, 2.0f * radius }, Origin::Center, 0.0f }.Raycast(
		ray, rect
	);*/
	/*V2_float b_min{ rect.Min() };
	V2_float b_max{ rect.Max() };
	auto r1 = Raycast(ray, Circle{ b_min, radius });
	auto r2 = Raycast(ray, Circle{ V2_float{ b_max.x, b_min.y }, radius });
	auto r3 = Raycast(ray, Circle{ b_max, radius });
	auto r4 = Raycast(ray, Circle{ V2_float{ b_min.x, b_max.y }, radius });*/

	ptgn::Raycast c;

	Line seg{ center, center + ray };

	/*bool start_inside{ Overlaps(rect) };
	bool end_inside{ rect.Overlaps(Circle{ seg.b, radius }) };*/

	// if (start_inside) {
	//	// Circle inside rectangle, flip segment direction.
	//	std::swap(seg.a, seg.b);
	// }

	// Compute the rectangle resulting from expanding b by circle radius.
	Rect e;
	e.position = rect.Min() - V2_float{ radius, radius };
	e.size	   = rect.size + V2_float{ radius * 2.0f, radius * 2.0f };
	e.origin   = Origin::TopLeft;

	/*if (!seg.Overlaps(e)) {
		return c;
	}*/

	V2_float b_min{ rect.Min() };
	V2_float b_max{ rect.Max() };

	ptgn::Raycast col_min{ c };
	// Top segment.
	auto c1{ seg.Raycast(Capsule{ { b_min, V2_float{ b_max.x, b_min.y } }, radius }) };
	if (c1.Occurred() && c1.t < col_min.t) {
		col_min = c1;
	}
	// Right segment.
	auto c2{ seg.Raycast(Capsule{ { V2_float{ b_max.x, b_min.y }, b_max }, radius }) };
	if (c2.Occurred() && c2.t < col_min.t) {
		col_min = c2;
	}
	// Bottom segment.
	auto c3{ seg.Raycast(Capsule{ { b_max, V2_float{ b_min.x, b_max.y } }, radius }) };
	if (c3.Occurred() && c3.t < col_min.t) {
		col_min = c3;
	}
	// Left segment.
	auto c4{ seg.Raycast(Capsule{ { V2_float{ b_min.x, b_max.y }, b_min }, radius }) };
	if (c4.Occurred() && c4.t < col_min.t) {
		col_min = c4;
	}

	if (col_min.t < 0.0f || col_min.t >= 1.0f) {
		return c;
	}

	// if (start_inside) {
	//	col_min.t = 1.0f - col_min.t;
	// }

	c = col_min;

	return c;
}

/*
void Arc::Draw(bool clockwise, const Color& color, float line_width, std::int32_t render_layer)
	const {
	PTGN_ASSERT(radius >= 0.0f, "Cannot draw filled arc with negative radius");

	// Edge case where arc is a point.
	if (NearlyEqual(radius, 0.0f)) {
		impl::Point::Draw(center.x, center.y, color.Normalized(), render_layer);
		return;
	}

	// Clamped start angle.
	float sa{ ClampAngle2Pi(start_angle) };

	// Clamped end angle.
	float ea{ ClampAngle2Pi(end_angle) };

	// Edge case where start and end angles match (considered a full rotation).
	if (float range{ sa - ea }; NearlyEqual(range, 0.0f) || NearlyEqual(range, two_pi<float>)) {
		Circle c{ center, radius };
		c.Draw(color, line_width, render_layer);
		return;
	}

	auto norm_color{ color.Normalized() };

	if (line_width == -1.0f) {
		DrawSolid(clockwise, sa, ea, norm_color, render_layer);
		return;
	}

	DrawThick(line_width, clockwise, sa, ea, norm_color, render_layer);
}

void Arc::DrawSolid(
	bool clockwise, float sa, float ea, const V4_float& color, std::int32_t render_layer
) const {
	auto vertices{ GetVertices(clockwise, sa, ea) };
	std::size_t vertex_count{ vertices.size() - 1 };
	for (std::size_t i{ 0 }; i < vertex_count; i++) {
		game.renderer.GetRenderData().AddPrimitiveTriangle(
			{ center, vertices[i], vertices[i + 1] }, render_layer, color
		);
	}
}

void Arc::DrawThick(
	float line_width, bool clockwise, float sa, float ea, const V4_float& color,
	std::int32_t render_layer
) const {
	auto vertices{ GetVertices(clockwise, sa, ea) };
	impl::DrawVertices(
		vertices.data(), vertices.size() - 1, line_width, color, render_layer, vertices.size()
	);
}
*/

std::vector<V2_float> Arc::GetVertices(bool clockwise, float sa, float ea) const {
	if (sa > ea) {
		ea += two_pi<float>;
	}

	float arc_angle{ ea - sa };

	PTGN_ASSERT(arc_angle >= 0.0f);

	// Resolution indicates the number of vertices the arc is made up of. Each consecutive vertex,
	// alongside the center of the arc, makes up a triangle which is used to draw solid arcs.
	std::size_t resolution{
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * radius))
	};

	PTGN_ASSERT(
		resolution > 1, "Arc must be made up of at least two vertices (forming one triangle with "
						"the arc center point)"
	);

	float delta_angle{ arc_angle / static_cast<float>(resolution) };

	std::vector<V2_float> vertices(resolution);

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		float angle{ start_angle };
		float delta{ static_cast<float>(i) * delta_angle };
		if (clockwise) {
			angle -= delta;
		} else {
			angle += delta;
		}

		vertices[i] = center + radius * V2_float{ std::cos(angle), std::sin(angle) };
	}

	return vertices;
}

} // namespace ptgn