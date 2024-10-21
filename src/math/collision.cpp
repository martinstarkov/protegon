#include "protegon/collision.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "protegon/circle.h"
#include "protegon/game.h"
#include "protegon/line.h"
#include "protegon/log.h"
#include "protegon/math.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"

namespace ptgn::impl {

float SquareDistancePointRectangle(const V2_float& a, const Rectangle<float>& b) {
	float dist2{ 0.0f };
	V2_float max{ b.Max() };
	V2_float min{ b.Min() };
	for (std::size_t i{ 0 }; i < 2; ++i) {
		const float v{ a[i] };
		if (v < min[i]) {
			dist2 += (min[i] - v) * (min[i] - v);
		}
		if (v > max[i]) {
			dist2 += (v - max[i]) * (v - max[i]);
		}
	}
	return dist2;
}

float ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c) {
	return (a - c).Cross(b - c);
}

std::vector<Axis> GetAxes(const Polygon& polygon, bool intersection_info) {
	std::vector<Axis> axes;

	const auto parallel_axis_exists = [&axes](const Axis& o_axis) {
		for (const auto& axis : axes) {
			if (NearlyEqual(o_axis.direction.Cross(axis.direction), 0.0f)) {
				return true;
			}
		}
		return false;
	};

	axes.reserve(polygon.vertices.size());

	for (std::size_t a{ 0 }; a < polygon.vertices.size(); a++) {
		std::size_t b{ a + 1 == polygon.vertices.size() ? 0 : a + 1 };

		Axis axis;
		axis.midpoint  = Midpoint(polygon.vertices[a], polygon.vertices[b]);
		axis.direction = polygon.vertices[a] - polygon.vertices[b];

		// Skip coinciding points with no axis.
		if (axis.direction.IsZero()) {
			continue;
		}

		axis.direction = axis.direction.Skewed();

		// TODO: Check if this still produces accurate results.
		// if (intersection_info) {
		axis.direction = axis.direction.Normalized();
		//}

		if (!parallel_axis_exists(axis)) {
			axes.emplace_back(axis);
		}
	}

	return axes;
}

std::pair<float, float> GetProjectionMinMax(const Polygon& polygon, const Axis& axis) {
	PTGN_ASSERT(polygon.vertices.size() > 0);
	PTGN_ASSERT(
		std::invoke([&]() {
			float mag2{ axis.direction.MagnitudeSquared() };
			return mag2 < 1.0f || NearlyEqual(mag2, 1.0f);
		}),
		"Projection axis must be normalized"
	);

	float min{ axis.direction.Dot(polygon.vertices.front()) };
	float max{ min };
	for (std::size_t i{ 1 }; i < polygon.vertices.size(); i++) {
		float p = polygon.vertices[i].Dot(axis.direction);
		if (p < min) {
			min = p;
		} else if (p > max) {
			max = p;
		}
	}

	return { min, max };
}

bool IntervalsOverlap(float min1, float max1, float min2, float max2) {
	return !(min1 > max2 || min2 > max1);
}

float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
) {
	// TODO: Combine the contained_polygon case into the regular overlap logic if possible.

	if (!IntervalsOverlap(min1, max1, min2, max2)) {
		return 0.0f;
	}

	float min_dist{ min1 - min2 };
	float max_dist{ max1 - max2 };

	if (contained_polygon) {
		float internal_dist{ std::min(max1, max2) - std::max(min1, min2) };

		// Get the overlap plus the distance from the minimum end points.
		float min_endpoint{ FastAbs(min_dist) };
		float max_endpoint{ FastAbs(max_dist) };

		if (max_endpoint > min_endpoint) {
			// Flip projection normal direction.
			out_axis_direction *= -1.0f;
			return internal_dist + min_endpoint;
		}
		return internal_dist + max_endpoint;
	}

	float right_dist{ FastAbs(min1 - max2) };

	if (max_dist > 0.0f) { // Overlapping the interval from the right.
		return right_dist;
	}

	float left_dist{ FastAbs(max1 - min2) };

	if (min_dist < 0.0f) { // Overlapping the interval from the left.
		return left_dist;
	}

	// Entirely within the interval.
	return std::min(right_dist, left_dist);
}

bool OverlapCollisionHandler::RectangleRectangle(
	const Rectangle<float>& a, const Rectangle<float>& b, float rotation_a, float rotation_b
) {
	if (rotation_a != 0.0f || rotation_b != 0.0f) {
		Polygon poly_a{ a, rotation_a };
		Polygon poly_b{ b, rotation_b };

		return poly_a.Overlaps(poly_b);
	}

	V2_float a_max{ a.Max() };
	V2_float a_min{ a.Min() };
	V2_float b_max{ b.Max() };
	V2_float b_min{ b.Min() };

	if (a_max.x < b_min.x || a_min.x > b_max.x) {
		return false;
	}

	if (a_max.y < b_min.y || a_min.y > b_max.y) {
		return false;
	}

	// Optional: Discount seam collisions:

	if (NearlyEqual(a_min.x, b_max.x) || NearlyEqual(a_max.x, b_min.x)) {
		return false;
	}

	if (NearlyEqual(a_max.y, b_min.y) || NearlyEqual(a_min.y, b_max.y)) {
		return false;
	}

	return true;
}

bool OverlapCollisionHandler::CircleCircle(const Circle<float>& a, const Circle<float>& b) {
	V2_float dist{ a.center - b.center };
	float dist2{ dist.Dot(dist) };
	float rad_sum{ a.radius + b.radius };
	float rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool OverlapCollisionHandler::CircleRectangle(const Circle<float>& a, const Rectangle<float>& b) {
	float dist2{ SquareDistancePointRectangle(a.center, b) };
	float rad2{ a.radius * a.radius };
	return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

bool OverlapCollisionHandler::PointRectangle(const V2_float& a, const Rectangle<float>& b) {
	V2_float b_max{ b.Max() };
	V2_float b_min{ b.Min() };

	if (a.x < b_min.x || a.x > b_max.x) {
		return false;
	}

	if (a.y < b_min.y || a.y > b_max.y) {
		return false;
	}

	// Optional: Discount seam collisions:

	if (NearlyEqual(a.x, b_max.x) || NearlyEqual(a.x, b_min.x)) {
		return false;
	}

	if (NearlyEqual(a.y, b_min.y) || NearlyEqual(a.y, b_max.y)) {
		return false;
	}

	return true;
}

bool OverlapCollisionHandler::PointCircle(const V2_float& a, const Circle<float>& b) {
	return CircleCircle({ a, 0.0f }, b);
}

bool OverlapCollisionHandler::PointSegment(const V2_float& a, const Segment<float>& b) {
	V2_float ab{ b.Direction() };
	V2_float ac{ a - b.a };
	V2_float bc{ a - b.b };

	float e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab
	if (e < 0 || NearlyEqual(e, 0.0f)) {
		return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);
	}

	float f{ ab.Dot(ab) };
	if (e > f || NearlyEqual(e, f)) {
		return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);
	}

	// Handle cases where c projects onto ab
	return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool OverlapCollisionHandler::SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b) {
	V2_float c = b.Center();		 // Box center-point
	V2_float e = b.Half();			 // Box halflength extents
	V2_float m = (a.a + a.b) * 0.5f; // Segment midpoint
	V2_float d = a.b - m;			 // Segment halflength vector
	m		   = m - c;				 // Translate box and segment to origin
	// Try world coordinate axes as separating axes
	float adx = FastAbs(d.x);
	if (FastAbs(m.x) >= e.x + adx) {
		return false;
	}
	float ady = FastAbs(d.y);
	if (FastAbs(m.y) >= e.y + ady) {
		return false;
	}
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis.
	adx += epsilon<float>;
	ady += epsilon<float>;

	// Try cross products of segment direction vector with coordinate axes.
	float cross = m.Cross(d);
	float dot{ e.Dot({ ady, adx }) };

	if (FastAbs(cross) > dot) {
		return false;
	}
	// No separating axis found; segment must be overlapping AABB.
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool OverlapCollisionHandler::SegmentCircle(const Segment<float>& a, const Circle<float>& b) {
	// If the line is inside the circle entirely, exit early.
	if (PointCircle(a.a, b) && PointCircle(a.b, b)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };
	float rad2{ b.radius * b.radius };

	// O is the circle center, P is the line origin, Q is the line destination.
	V2_float OP{ a.a - b.center };
	V2_float OQ{ a.b - b.center };
	V2_float PQ{ a.Direction() };

	float OP_dist2{ OP.Dot(OP) };
	float OQ_dist2{ OQ.Dot(OQ) };
	float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		float triangle_area{ FastAbs(ParallelogramArea(b.center, a.a, a.b)) / 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
		   (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}

bool OverlapCollisionHandler::SegmentSegment(const Segment<float>& a, const Segment<float>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	float a1{ ParallelogramArea(a.a, a.b, b.b) }; // Compute winding of abd (+ or -)
	float a2{ ParallelogramArea(a.a, a.b, b.a) }; // To intersect, must have sign opposite of a1
	// If c and d are on different sides of ab, areas have different signs
	bool polarity_diff{ false };
	bool collinear{ false };
	// Same as above but for floating points.
	polarity_diff = a1 * a2 < 0.0f;
	collinear	  = NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f);
	// For integral implementation use this instead of the above two lines:
	// if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
	//	// Second part for difference in polarity.
	//	polarity_diff = (a1 ^ a2) < 0;
	//	collinear = a1 == 0 || a2 == 0;
	//}
	if (!collinear && polarity_diff) {
		// Compute signs for a and b with respect to segment cd
		float a3{ ParallelogramArea(b.a, b.b, a.a) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite
		// sign of a3
		float a4{ a3 + a2 - a1 };
		// Points a and b on different sides of cd if areas have different signs
		// Segments intersect if true.
		bool intersect{ false };
		// If either is 0, the line is intersecting with the straight edge of
		// the other line. (i.e. corners with angles). Check if a3 and a4 signs
		// are different.
		intersect = a3 * a4 < 0.0f;
		collinear = NearlyEqual(a3, 0.0f) || NearlyEqual(a4, 0.0f);
		// For integral implementation use this instead of the above two lines:
		// if constexpr (std::is_signed_v<T> && std::is_integral_v<T>) {
		//	intersect = (a3 ^ a4) < 0;
		//	collinear = a3 == 0 || a4 == 0;
		//}
		if (intersect) {
			return true;
		}
	}
	return collinear && (PointSegment(b.b, a) || PointSegment(b.a, a) || PointSegment(a.a, b) ||
						 PointSegment(a.b, b));
}

bool IntersectCollisionHandler::CircleCircle(
	const Circle<float>& a, const Circle<float>& b, IntersectCollision& c
) {
	c = {};

	V2_float d{ b.center - a.center };
	float dist2{ d.Dot(d) };
	float r{ a.radius + b.radius };

	if (dist2 > r * r) {
		return false;
	}

	// Edge case where circle centers are in the same location.
	c.normal = { 0.0f, -1.0f }; // upward
	c.depth	 = r;

	if (dist2 > epsilon2<float>) {
		float dist{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(dist, 0.0f));
		c.normal = -d / dist;
		c.depth	 = r - dist;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return true;
}

bool IntersectCollisionHandler::RectangleRectangle(
	const Rectangle<float>& a, const Rectangle<float>& b, IntersectCollision& c, float rotation_a,
	float rotation_b
) {
	if (rotation_a != 0.0f || rotation_b != 0.0f) {
		Polygon poly_a{ a, rotation_a };
		Polygon poly_b{ b, rotation_b };
		return PolygonPolygon(poly_a, poly_b, c);
	}

	c = {};

	V2_float a_h{ a.Half() };
	V2_float b_h{ b.Half() };
	V2_float d{ b.Center() - a.Center() };
	V2_float pen{ a_h + b_h - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

	if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f)) {
		return false;
	}

	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		// Edge case where aabb centers are in the same location.
		c.normal = { 0.0f, -1.0f }; // upward
		c.depth	 = a_h.y + b_h.y;
	} else if (pen.y < pen.x) {
		c.normal = { 0.0f, -Sign(d.y) };
		c.depth	 = FastAbs(pen.y);
	} else {
		c.normal = { -Sign(d.x), 0.0f };
		c.depth	 = FastAbs(pen.x);
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return true;
}

bool IntersectCollisionHandler::PolygonPolygon(
	const Polygon& r1, const Polygon& r2, IntersectCollision& c
) {
	const auto minimum_overlap = [](const Polygon& p1, const Polygon& p2,
									const std::vector<Axis>& axes, float& depth, Axis& axis) {
		for (Axis a : axes) {
			auto [min1, max1] = GetProjectionMinMax(p1, a);
			auto [min2, max2] = GetProjectionMinMax(p2, a);

			if (!IntervalsOverlap(min1, max1, min2, max2)) {
				return false;
			}
			bool contained{ p1.Contains(p2) || p2.Contains(p1) };

			float o{ GetIntervalOverlap(min1, max1, min2, max2, contained, axis.direction) };

			if (o < depth) {
				depth = o;
				axis  = a;
			}
		}
		return true;
	};

	c = {};

	float depth{ std::numeric_limits<float>::infinity() };
	Axis axis;

	if (!minimum_overlap(r1, r2, GetAxes(r1, true), depth, axis) ||
		!minimum_overlap(r2, r1, GetAxes(r2, true), depth, axis)) {
		return false;
	}

	// Make sure the vector is pointing from polygon1 to polygon2.
	if (V2_float dir{ r1.GetCentroid() - r2.GetCentroid() }; dir.Dot(axis.direction) < 0) {
		axis.direction *= -1.0f;
	}

	c.normal = axis.direction;
	c.depth	 = depth;

	// Useful debugging code:

	// Draw all polygon points projected onto all the axes.
	const auto draw_axes = [](const std::vector<Axis>& axes, const Polygon& polygon) {
		for (const auto& a : axes) {
			game.draw.Axis(a.midpoint, a.direction, color::Pink, 1.0f);
			auto [min, max] = GetProjectionMinMax(polygon, a);
			V2_float p1{ a.midpoint + a.direction * min };
			V2_float p2{ a.midpoint + a.direction * max };
			game.draw.Point(p1, color::Purple, 5.0f);
			game.draw.Point(p2, color::Orange, 5.0f);
			V2_float to{ 0.0f, -17.0f };
			game.draw.Text(std::to_string((int)min), p1 + to, color::Purple);
			game.draw.Text(std::to_string((int)max), p2 + to, color::Orange);
		}
	};

	draw_axes(GetAxes(r1, true), r1);
	draw_axes(GetAxes(r1, true), r2);
	draw_axes(GetAxes(r2, true), r1);
	draw_axes(GetAxes(r2, true), r2);

	// Draw overlap axis and overlap amounts on both sides.
	game.draw.Axis(axis.midpoint, c.normal, color::Black, 2.0f);
	game.draw.Line(axis.midpoint, axis.midpoint - c.normal * c.depth, color::Cyan, 3.0f);
	game.draw.Line(axis.midpoint, axis.midpoint + c.normal * c.depth, color::Cyan, 3.0f);

	return true;
}

bool IntersectCollisionHandler::CircleRectangle(
	const Circle<float>& a, const Rectangle<float>& b, IntersectCollision& c
) {
	c = {};

	V2_float half{ b.Half() };
	V2_float b_max{ b.Max() };
	V2_float b_min{ b.Min() };
	V2_float clamped{ std::clamp(a.center.x, b_min.x, b_max.x),
					  std::clamp(a.center.y, b_min.y, b_max.y) };
	V2_float ab{ a.center - clamped };

	float d2{ ab.Dot(ab) };

	if (float r2{ a.radius * a.radius }; d2 < r2) {
		if (NearlyEqual(d2, 0.0f)) { // deep (center of circle inside of AABB)

			// clamp circle's center to edge of AABB, then form the manifold
			V2_float mid{ b.Center() };
			V2_float d{ mid - a.center };

			V2_float overlap{ half - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

			if (overlap.x < overlap.y) {
				c.depth	 = a.radius + overlap.x;
				c.normal = V2_float{ 1.0f, 0.0f } * (d.x < 0 ? 1.0f : -1.0f);
			} else {
				c.depth	 = a.radius + overlap.y;
				c.normal = V2_float{ 0.0f, 1.0f } * (d.y < 0 ? 1.0f : -1.0f);
			}
		} else { // shallow (center of circle not inside of AABB)
			float d{ std::sqrt(d2) };
			PTGN_ASSERT(!NearlyEqual(d, 0.0f));
			c.normal = ab / d;
			c.depth	 = a.radius - d;
		}

		PTGN_ASSERT(c.depth >= 0.0f);

		return true;
	}
	return false;
}

bool DynamicCollisionHandler::SegmentSegment(
	const Segment<float>& a, const Segment<float>& b, DynamicCollision& c
) {
	c = {};

	if (!OverlapCollisionHandler::SegmentSegment(a, b)) {
		return false;
	}

	V2_float r{ a.Direction() };
	V2_float s{ b.Direction() };

	float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return false;
	}

	V2_float ab{ a.a - b.a };
	float abr{ ab.Cross(r) };

	if (float u{ abr / sr }; u < 0.0f || u > 1.0f) {
		return false;
	}

	V2_float ba{ b.a - a.a };
	float rs{ r.Cross(s) };
	if (NearlyEqual(rs, 0.0f)) {
		return false;
	}

	V2_float skewed{ -s.Skewed() };
	float mag2{ skewed.Dot(skewed) };
	if (NearlyEqual(mag2, 0.0f)) {
		return false;
	}

	float bas{ ba.Cross(s) };
	float t{ bas / rs };

	if (t < 0.0f || t > 1.0f) {
		return false;
	}

	c.t		 = t;
	c.normal = skewed / std::sqrt(mag2);
	return true;
}

bool DynamicCollisionHandler::SegmentCircle(
	const Segment<float>& a, const Circle<float>& b, DynamicCollision& c
) {
	c = {};

	if (!OverlapCollisionHandler::SegmentCircle(a, b)) {
		return false;
	}

	V2_float d{ -a.Direction() };
	V2_float f{ b.center - a.a };

	// bool (roots exist), float (root 1), float (root 2).
	auto [real, t1, t2] =
		QuadraticFormula(d.Dot(d), 2.0f * f.Dot(d), f.Dot(f) - b.radius * b.radius);

	if (!real) {
		return false;
	}

	bool w1{ t1 >= 0.0f && t1 <= 1.0f };
	bool w2{ t2 >= 0.0f && t2 <= 1.0f };

	// Pick the lowest collision time that is in the [0, 1] range.
	if (w1 && w2) {
		c.t = std::min(t1, t2);
	} else if (w1) {
		c.t = t1;
	} else if (w2) {
		c.t = t2;
	} else {
		return false;
	}

	V2_float impact{ b.center + d * c.t - a.a };

	float mag2{ impact.Dot(impact) };

	if (NearlyEqual(mag2, 0.0f)) {
		c = {};
		return false;
	}

	c.normal = -impact / std::sqrt(mag2);

	return true;
}

bool DynamicCollisionHandler::SegmentRectangle(
	const Segment<float>& a, Rectangle<float> b, DynamicCollision& c
) {
	c = {};

	bool start_in{ OverlapCollisionHandler::PointRectangle(a.a, b) };
	bool end_in{ OverlapCollisionHandler::PointRectangle(a.b, b) };

	if (start_in && end_in) {
		return false;
	}

	V2_float d{ a.Direction() };

	if (d.Dot(d) == 0.0f) {
		return false;
	}

	// TODO: Deal with situation where rectangle is inside the other rectangle.

	// Cache division.
	V2_float inv_dir{ 1.0f / d };

	// Calculate intersections with rectangle bounding axes.
	V2_float near{ b.Min() - a.a };
	V2_float far{ b.Max() - a.a };

	// Handle edge cases where the segment line is parallel with the edge of the rectangle.
	if (NearlyEqual(near.x, 0.0f)) {
		near.x = 0.0f;
	}
	if (NearlyEqual(near.y, 0.0f)) {
		near.y = 0.0f;
	}
	if (NearlyEqual(far.x, 0.0f)) {
		far.x = 0.0f;
	}
	if (NearlyEqual(far.y, 0.0f)) {
		far.y = 0.0f;
	}

	V2_float t_near{ near * inv_dir };
	V2_float t_far{ far * inv_dir };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return false;
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return false;
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x >= t_far.y || t_near.y >= t_far.x) {
		return false;
	}

	// Furthest time is contact on opposite side of target.
	// Reject if furthest time is negative, meaning the object is travelling away from the
	// target.
	float t_hit_far = std::min(t_far.x, t_far.y);
	if (t_hit_far < 0.0f) {
		return false;
	}

	if (NearlyEqual(t_near.x, t_near.y) && t_near.x == 1.0f) {
		return false;
	}

	// Closest time will be the first contact.
	bool interal{ start_in && !end_in };

	if (interal) {
		std::swap(t_near.x, t_far.x);
		std::swap(t_near.y, t_far.y);
		std::swap(inv_dir.x, inv_dir.y);
		c.t	 = std::min(t_near.x, t_near.y);
		d	*= -1.0f;
	} else {
		c.t = std::max(t_near.x, t_near.y);
	}

	if (c.t > 1.0f) {
		c = {};
		return false;
	}

	// Contact point of collision from parametric line equation.
	// c.point = a.a + c.time * d;

	// Find which axis collides further along the movement time.

	// TODO: Figure out how to fix biasing of one direction from one side and another on the
	// other side.
	bool equal_times{ NearlyEqual(t_near.x, t_near.y) };
	bool diagonal{ NearlyEqual(FastAbs(inv_dir.x), FastAbs(inv_dir.y)) };

	if (equal_times && diagonal) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		c.normal = { -Sign(d.x), -Sign(d.y) };
	}
	if (c.normal.IsZero()) {
		if (t_near.x > t_near.y) { // X-axis.
			// Direction of movement.
			if (inv_dir.x < 0.0f) {
				c.normal = { 1.0f, 0.0f };
			} else {
				c.normal = { -1.0f, 0.0f };
			}
		} else if (t_near.x < t_near.y) { // Y-axis.
			// Direction of movement.
			if (inv_dir.y < 0.0f) {
				c.normal = { 0.0f, 1.0f };
			} else {
				c.normal = { 0.0f, -1.0f };
			}
		}
	}

	if (interal) {
		std::swap(c.normal.x, c.normal.y);
		c.normal *= -1.0f;
	}

	// Raycast collision occurred.
	return true;
}

bool DynamicCollisionHandler::SegmentCapsule(
	const Segment<float>& a, const Capsule<float>& b, DynamicCollision& c
) {
	c = {};

	// TODO: Add early exit if overlap test fails.

	V2_float cv{ b.segment.Direction() };
	float mag2{ cv.Dot(cv) };

	if (NearlyEqual(mag2, 0.0f)) {
		return SegmentCircle(a, { b.segment.a, b.radius }, c);
	}

	float mag{ std::sqrt(mag2) };
	V2_float cu{ cv / mag };
	// Normal to b.segment
	V2_float ncu{ cu.Skewed() };
	V2_float ncu_dist{ ncu * b.radius };

	Segment<float> p1{ b.segment.a + ncu_dist, b.segment.b + ncu_dist };
	Segment<float> p2{ b.segment.a - ncu_dist, b.segment.b - ncu_dist };

	DynamicCollision col_min{ c };
	if (SegmentSegment(a, p1, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (SegmentSegment(a, p2, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (SegmentCircle(a, { b.segment.a, b.radius }, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (SegmentCircle(a, { b.segment.b, b.radius }, c) && c.t < col_min.t) {
		col_min = c;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		c = {};
		return false;
	}

	c = col_min;
	return true;
}

bool DynamicCollisionHandler::CircleCircle(
	const Circle<float>& a, const V2_float& vel, const Circle<float>& b, DynamicCollision& c
) {
	return SegmentCircle({ a.center, a.center + vel }, { b.center, b.radius + a.radius }, c);
}

bool DynamicCollisionHandler::CircleRectangle(
	const Circle<float>& a, const V2_float& vel, const Rectangle<float>& b, DynamicCollision& c
) {
	c = {};

	Segment<float> seg{ a.center, a.center + vel };

	bool start_inside{ OverlapCollisionHandler::CircleRectangle(a, b) };
	bool end_inside{ OverlapCollisionHandler::CircleRectangle({ seg.b, a.radius }, b) };

	if (start_inside && end_inside) {
		return false;
	}

	if (start_inside) {
		// Circle inside rectangle, flip segment direction.
		std::swap(seg.a, seg.b);
	}

	// Compute the rectangle resulting from expanding b by circle radius.
	Rectangle<float> e;
	e.pos	 = b.Min() - V2_float{ a.radius, a.radius };
	e.size	 = b.size + V2_float{ a.radius * 2.0f, a.radius * 2.0f };
	e.origin = Origin::TopLeft;

	if (!OverlapCollisionHandler::SegmentRectangle(seg, e)) {
		return false;
	}

	V2_float b_min{ b.Min() };
	V2_float b_max{ b.Max() };

	DynamicCollision col_min{ c };
	// Top segment.
	if (SegmentCapsule(seg, { { b_min, V2_float{ b_max.x, b_min.y } }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Right segment.
	if (SegmentCapsule(seg, { { V2_float{ b_max.x, b_min.y }, b_max }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Bottom segment.
	if (SegmentCapsule(seg, { { b_max, V2_float{ b_min.x, b_max.y } }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Left segment.
	if (SegmentCapsule(seg, { { V2_float{ b_min.x, b_max.y }, b_min }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		return false;
	}

	if (start_inside) {
		col_min.t = 1.0f - col_min.t;
	}

	c = col_min;

	return true;
}

bool DynamicCollisionHandler::RectangleRectangle(
	const Rectangle<float>& a, const V2_float& vel, const Rectangle<float>& b, DynamicCollision& c
) {
	V2_float a_center{ a.Center() };
	bool occured = SegmentRectangle(
		{ a_center, a_center + vel }, { b.Min() - a.Half(), b.size + a.size, Origin::TopLeft }, c
	);
	bool collide_on_next_frame{ c.t < 1.0 && c.t >= 0.0f };
	return occured && collide_on_next_frame && !c.normal.IsZero();
}

V2_float DynamicCollisionHandler::Sweep(
	ecs::Entity entity, ecs::Manager& manager,
	DynamicCollisionResponse response = DynamicCollisionResponse::Slide, bool debug_draw
) {
	float dt = game.dt();

	PTGN_ASSERT(dt > 0.0f);

	if (!entity.Has<Transform, RigidBody>()) {
		return {};
	}

	bool is_circle{ entity.Has<CircleCollider>() };
	bool is_rectangle{ entity.Has<BoxCollider>() };

	if (!is_circle && !is_rectangle) {
		return {};
	}

	const auto& rigid_body = entity.Get<RigidBody>();

	const auto velocity = rigid_body.velocity * dt;

	if (velocity.IsZero()) {
		// TODO: Consider adding a static intersect check here.
		return rigid_body.velocity;
	}

	const auto& transform = entity.Get<Transform>();

	DynamicCollision c;

	auto targets = manager.EntitiesWith<Transform>();

	auto collision_occurred = [&](const V2_float& pos, const V2_float& vel, ecs::Entity e,
								  float& dist2) {
		PTGN_ASSERT(e.Has<Transform>());
		if (!e.HasAny<BoxCollider, CircleCollider>()) {
			return false;
		}
		V2_float relative_velocity = vel;
		if (e.Has<RigidBody>()) {
			relative_velocity -= e.Get<RigidBody>().velocity * dt;
		}
		const auto& transform2 = e.Get<Transform>();
		if (is_rectangle) {
			const auto& box = entity.Get<BoxCollider>();
			Rectangle rect{ pos + box.offset, box.size, box.origin };
			if (e.Has<BoxCollider>()) {
				const auto& box2 = e.Get<BoxCollider>();
				Rectangle rect2{ transform2.position + box2.offset, box2.size, box2.origin };
				dist2 = (rect.Center() - rect2.Center()).MagnitudeSquared();
				return RectangleRectangle(rect, relative_velocity, rect2, c);
			} else if (e.Has<CircleCollider>()) {
				const auto& circle2 = e.Get<CircleCollider>();
				Circle c2{ transform2.position + circle2.offset, circle2.radius };
				dist2 = (rect.Center() - c2.center).MagnitudeSquared();
				return CircleRectangle(c2, -relative_velocity, rect, c);
			}
		} else if (is_circle) {
			const auto& circle = entity.Get<CircleCollider>();
			Circle c1{ pos + circle.offset, circle.radius };
			if (e.Has<BoxCollider>()) {
				const auto& box2 = e.Get<BoxCollider>();
				Rectangle rect2{ transform2.position + box2.offset, box2.size, box2.origin };
				dist2 = (c1.center - rect2.Center()).MagnitudeSquared();
				return CircleRectangle(c1, relative_velocity, rect2, c);
			} else if (e.Has<CircleCollider>()) {
				const auto& circle2 = e.Get<CircleCollider>();
				Circle c2{ transform2.position + circle2.offset, circle2.radius };
				dist2 = (c1.center - c2.center).MagnitudeSquared();
				return CircleCircle(c1, -relative_velocity, c2, c);
			}
		}
		PTGN_ERROR("Unrecognized shape for collision check");
	};

	auto get_sorted_collisions = [&](const V2_float& pos, const V2_float& vel) {
		std::vector<SweepCollision> collisions;
		targets.ForEach([&](ecs::Entity e) {
			if (entity == e) {
				return;
			}
			float dist2{ 0.0f };
			if (collision_occurred(pos, vel, e, dist2)) {
				collisions.emplace_back(c, dist2);
			}
		});
		SortCollisions(collisions);
		return collisions;
	};

	const auto collisions = get_sorted_collisions(transform.position, velocity);

	if (collisions.empty()) { // no collisions occured.
		const auto new_p1 = transform.position + velocity;
		if (debug_draw) {
			game.draw.Line(transform.position, new_p1, color::Grey);
		}
		return rigid_body.velocity;
	}

	const auto new_velocity = GetRemainingVelocity(velocity, collisions[0].c, response);

	const auto new_p1 = transform.position + velocity * collisions[0].c.t;

	PTGN_ASSERT(entity.Has<BoxCollider>());

	if (debug_draw) {
		game.draw.Line(transform.position, new_p1, color::Blue);
		game.draw.Rectangle(
			new_p1, entity.Get<BoxCollider>().size, color::Purple, entity.Get<BoxCollider>().origin,
			1.0f
		);
	}

	if (new_velocity.IsZero()) {
		return rigid_body.velocity * collisions[0].c.t;
	}

	if (const auto collisions2 = get_sorted_collisions(new_p1, new_velocity);
		!collisions2.empty()) {
		if (debug_draw) {
			game.draw.Line(new_p1, new_p1 + new_velocity * collisions2[0].c.t, color::Green);
		}
		return rigid_body.velocity * collisions[0].c.t + new_velocity * collisions2[0].c.t / dt;
	}
	if (debug_draw) {
		game.draw.Line(new_p1, new_p1 + new_velocity, color::Orange);
	}
	return rigid_body.velocity * collisions[0].c.t + new_velocity / dt;
}

void DynamicCollisionHandler::SortCollisions(std::vector<SweepCollision>& collisions) {
	/*
	 * Initial sort based on distances of collision manifolds to the collider.
	 * This is required for RectangleVsRectangle collisions to prevent sticking
	 * to corners in certain configurations, such as if the player (o) gives
	 * a bottom right velocity into the following rectangle (x) configuration:
	 *       x
	 *     o x
	 *   x   x
	 * (player would stay still instead of moving down if this distance sort did not exist).
	 */
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) { return a.dist2 < b.dist2; }
	);
	// Sort based on collision times, and if they are equal, by collision normal magnitudes.
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) {
			// If time of collision are equal, prioritize walls to corners, i.e. normals
			// (1,0) come before (1,1).
			if (a.c.t == b.c.t) {
				return a.c.normal.MagnitudeSquared() < b.c.normal.MagnitudeSquared();
			}
			// If collision times are not equal, sort by collision time.
			return a.c.t < b.c.t;
		}
	);
}

V2_float DynamicCollisionHandler::GetRemainingVelocity(
	const V2_float& velocity, const DynamicCollision& c, DynamicCollisionResponse response
) {
	float remaining_time{ 1.0f - c.t };

	switch (response) {
		case DynamicCollisionResponse::Slide: {
			V2_float tangent{ -c.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		}
		case DynamicCollisionResponse::Push: {
			return Sign(velocity.Dot(-c.normal.Skewed())) * c.normal.Swapped() * remaining_time *
				   velocity.Magnitude();
		}
		case DynamicCollisionResponse::Bounce: {
			V2_float new_velocity = velocity * remaining_time;
			if (!NearlyEqual(FastAbs(c.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(FastAbs(c.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		}
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Shutdown() {
	overlap	  = {};
	intersect = {};
	dynamic	  = {};
}

} // namespace ptgn::impl