#include "protegon/collision.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "protegon/circle.h"
#include "protegon/line.h"
#include "protegon/log.h"
#include "protegon/math.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"

namespace ptgn {

float OverlapCollision::SquareDistancePointRectangle(const V2_float& a, const Rectangle<float>& b) {
	float dist2{ 0.0f };
	const V2_float max{ b.Max() };
	const V2_float min{ b.Min() };
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

float OverlapCollision::ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c) {
	return (a - c).Cross(b - c);
}

bool OverlapCollision::RectangleRectangle(const Rectangle<float>& a, const Rectangle<float>& b) {
	const V2_float a_max{ a.Max() };
	const V2_float a_min{ a.Min() };
	const V2_float b_max{ b.Max() };
	const V2_float b_min{ b.Min() };

	if (a_max.x < b_min.x || a_min.x > b_max.x) {
		return false;
	}
	if (a_max.y < b_min.y || a_min.y > b_max.y) {
		return false;
	}
	return true;
}

bool OverlapCollision::CircleCircle(const Circle<float>& a, const Circle<float>& b) {
	const V2_float dist{ a.center - b.center };
	const float dist2{ dist.Dot(dist) };
	const float rad_sum{ a.radius + b.radius };
	const float rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool OverlapCollision::CircleRectangle(const Circle<float>& a, const Rectangle<float>& b) {
	const float dist2{ SquareDistancePointRectangle(a.center, b) };
	const float rad2{ a.radius * a.radius };
	return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

bool OverlapCollision::PointRectangle(const V2_float& a, const Rectangle<float>& b) {
	return RectangleRectangle(Rectangle<float>{ a, {}, Origin::Center }, b);
}

bool OverlapCollision::PointCircle(const V2_float& a, const Circle<float>& b) {
	return CircleCircle({ a, 0.0f }, b);
}

bool OverlapCollision::PointSegment(const V2_float& a, const Segment<float>& b) {
	const V2_float ab{ b.Direction() };
	const V2_float ac{ a - b.a };
	const V2_float bc{ a - b.b };

	const float e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab
	if (e < 0 || NearlyEqual(e, 0.0f)) {
		return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);
	}

	const float f{ ab.Dot(ab) };
	if (e > f || NearlyEqual(e, f)) {
		return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);
	}

	// Handle cases where c projects onto ab
	return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool OverlapCollision::SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b) {
	const V2_float b_max{ b.Max() };
	const V2_float b_min{ b.Min() };

	V2_float c = (b_min + b_max) * 0.5f; // Box center-point
	V2_float e = b_max - c;				 // Box halflength extents
	V2_float m = (a.a + a.b) * 0.5f;	 // Segment midpoint
	V2_float d = a.b - m;				 // Segment halflength vector
	m		   = m - c;					 // Translate box and segment to origin
	// Try world coordinate axes as separating axes
	float adx = FastAbs(d.x);
	if (FastAbs(m.x) > e.x + adx) {
		return false;
	}
	float ady = FastAbs(d.y);
	if (FastAbs(m.y) > e.y + ady) {
		return false;
	}
	// Add in an epsilon term to counteract arithmetic errors when segment is
	// (near) parallel to a coordinate axis.
	adx += epsilon<float>;
	ady += epsilon<float>;

	// Try cross products of segment direction vector with coordinate axes.
	if (FastAbs(m.Cross(d)) > e.x * ady + e.y * adx) {
		return false;
	}
	// No separating axis found; segment must be overlapping AABB.
	return true;

	// Alternative method:
	// Source: https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
}

bool OverlapCollision::SegmentCircle(const Segment<float>& a, const Circle<float>& b) {
	// If the line is inside the circle entirely, exit early.
	if (PointCircle(a.a, b) && PointCircle(a.b, b)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };
	const float rad2{ b.radius * b.radius };

	// O is the circle center, P is the line origin, Q is the line destination.
	const V2_float OP{ a.a - b.center };
	const V2_float OQ{ a.b - b.center };
	const V2_float PQ{ a.Direction() };

	const float OP_dist2{ OP.Dot(OP) };
	const float OQ_dist2{ OQ.Dot(OQ) };
	const float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		const float triangle_area{ FastAbs(ParallelogramArea(b.center, a.a, a.b)) / 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
		   (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}

bool OverlapCollision::SegmentSegment(const Segment<float>& a, const Segment<float>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	const float a1{ ParallelogramArea(a.a, a.b, b.b) }; // Compute winding of abd (+ or -)
	const float a2{
		ParallelogramArea(a.a, a.b, b.a)
	}; // To intersect, must have sign opposite of a1
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
		const float a3{ ParallelogramArea(b.a, b.b, a.a) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite
		// sign of a3
		const float a4{ a3 + a2 - a1 };
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

	const V2_float d{ b.center - a.center };
	const float dist2{ d.Dot(d) };
	const float r{ a.radius + b.radius };

	if (dist2 > r * r) {
		return false;
	}

	// Edge case where circle centers are in the same location.
	c.normal = { 0.0f, -1.0f }; // upward
	c.depth	 = r;

	if (dist2 > epsilon2<float>) {
		const float dist{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(dist, 0.0f));
		c.normal = -d / dist;
		c.depth	 = r - dist;
	}
	return true;
}

bool IntersectCollisionHandler::RectangleRectangle(
	const Rectangle<float>& a, const Rectangle<float>& b, IntersectCollision& c
) {
	c = {};

	const V2_float a_h{ a.Half() };
	const V2_float b_h{ b.Half() };
	const V2_float d{ b.Center() - a.Center() };
	const V2_float pen{ a_h + b_h - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

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
	return true;
}

bool IntersectCollisionHandler::CircleRectangle(
	const Circle<float>& a, const Rectangle<float>& b, IntersectCollision& c
) {
	c = {};

	const V2_float half{ b.Half() };
	const V2_float b_max{ b.Max() };
	const V2_float b_min{ b.Min() };
	const V2_float clamped{ std::clamp(a.center.x, b_min.x, b_max.x),
							std::clamp(a.center.y, b_min.y, b_max.y) };
	const V2_float ab{ a.center - clamped };

	const float d2{ ab.Dot(ab) };

	if (const float r2{ a.radius * a.radius }; d2 < r2) {
		if (NearlyEqual(d2, 0.0f)) { // deep (center of circle inside of AABB)

			// clamp circle's center to edge of AABB, then form the manifold
			const V2_float mid{ b.Center() };
			const V2_float d{ mid - a.center };

			const float x_overlap{ half.x - FastAbs(d.x) };
			const float y_overlap{ half.y - FastAbs(d.y) };

			if (x_overlap < y_overlap) {
				c.depth	 = a.radius + x_overlap;
				c.normal = { 1.0f, 0.0f };
				c.normal = c.normal * (d.x < 0 ? 1.0f : -1.0f);
			} else {
				c.depth	 = a.radius + y_overlap;
				c.normal = { 0.0f, 1.0f };
				c.normal = c.normal * (d.y < 0 ? 1.0f : -1.0f);
			}
		} else { // shallow (center of circle not inside of AABB)
			const float d{ std::sqrt(d2) };
			PTGN_ASSERT(!NearlyEqual(d, 0.0f));
			c.normal = ab / d;
			c.depth	 = a.radius - d;
		}
		return true;
	}
	return false;
}

bool DynamicCollisionHandler::SegmentSegment(
	const Segment<float>& a, const Segment<float>& b, DynamicCollision& c
) {
	c = {};

	const V2_float r{ a.Direction() };
	const V2_float s{ b.Direction() };

	const float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return false;
	}

	const V2_float ab{ a.a - b.a };
	const float abr{ ab.Cross(r) };

	if (const float u{ abr / sr }; u < 0.0f || u > 1.0f) {
		return false;
	}

	const V2_float ba{ b.a - a.a };
	const float rs{ r.Cross(s) };
	if (NearlyEqual(rs, 0.0f)) {
		return false;
	}

	const V2_float skewed{ -s.Skewed() };
	const float mag2{ skewed.Dot(skewed) };
	if (NearlyEqual(mag2, 0.0f)) {
		return false;
	}

	const float bas{ ba.Cross(s) };

	const float t{ bas / rs };

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

	const V2_float d{ -a.Direction() };
	const V2_float f{ b.center - a.a };

	// bool (roots exist), float (root 1), float (root 2).
	const auto [real, t1, t2] =
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

	const V2_float impact{ b.center + d * c.t - a.a };

	const float mag2{ impact.Dot(impact) };

	if (NearlyEqual(mag2, 0.0f)) {
		c = {};
		return false;
	}

	c.normal = -impact / std::sqrt(mag2);

	return true;
}

bool DynamicCollisionHandler::SegmentRectangle(
	const Segment<float>& a, const Rectangle<float>& b, DynamicCollision& c
) {
	c = {};

	const V2_float d{ a.Direction() };

	if (NearlyEqual(d.Dot(d), 0.0f)) {
		return false;
	}

	auto occurred = [](const DynamicCollision& c) {
		return (c.t >= 0.0f && c.t < 1.0 || c.t > -1.0 && c.t < 0.0f) && !c.normal.IsZero();
	};

	const V2_float b_min{ b.Min() };
	const V2_float b_max{ b.Max() };

	V2_float inv_dir = 1.0f / d;
	V2_float t_near	 = (b_min - a.a) * inv_dir;
	V2_float t_far	 = (b_max - a.a) * inv_dir;

	if (OverlapCollision::PointRectangle(a.a, b)) {
		const float lo{ std::max(std::min(t_near.x, t_far.x), std::min(t_near.y, t_far.y)) };
		const float hi{ std::min(std::max(t_near.x, t_far.x), std::max(t_near.y, t_far.y)) };

		if (hi < 0.0f || hi < lo || lo > 1.0f) {
			return false;
		}

		// Pick the lowest collision time that is in the [0, 1] range.
		bool w1{ hi >= 0.0f && hi <= 1.0f };
		bool w2{ lo >= 0.0f && lo <= 1.0f };

		if (w1 && w2) {
			c.t = std::min(hi, lo);
		} else if (w1) {
			c.t = hi;
		} else if (w2) {
			c.t = lo;
		} else {
			return false;
		}

		const V2_float coeff{ a.a + d * c.t - (b_min + b_max) * 0.5f };
		const V2_float abs_coeff{ FastAbs(coeff.x), FastAbs(coeff.y) };

		if (NearlyEqual(abs_coeff.x, abs_coeff.y) &&
			NearlyEqual(FastAbs(inv_dir.x), FastAbs(inv_dir.y))) {
			c.normal = { Sign(coeff.x), Sign(coeff.y) };
		} else if (abs_coeff.x > abs_coeff.y) {
			c.normal = { Sign(coeff.x), 0.0f };
		} else {
			c.normal = { 0.0f, Sign(coeff.y) };
		}

		return std::invoke(occurred, c);
	}

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
	if (t_near.x > t_far.y || t_near.y > t_far.x) {
		return false;
	}

	// Closest time will be the first contact.
	c.t = std::max(t_near.x, t_near.y);

	// Furthest time is contact on opposite side of target.
	// Reject if furthest time is negative, meaning the object is travelling away from the
	// target.
	if (float t_hit_far = std::min(t_far.x, t_far.y); t_hit_far < 0.0f) {
		return false;
	}

	// Contact point of collision from parametric line equation.
	// c.point = a.a + c.time * d;

	// Find which axis collides further along the movement time.

	// TODO: Figure out how to fix biasing of one direction from one side and another on the
	// other side.
	if (NearlyEqual(t_near.x, t_near.y) && NearlyEqual(
											   FastAbs(inv_dir.x), FastAbs(inv_dir.y)
										   )) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		c.normal = { -Sign(d.x), -Sign(d.y) };
	} else if (t_near.x > t_near.y) { // X-axis.
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

	return std::invoke(occurred, c);
}

bool DynamicCollisionHandler::SegmentCapsule(
	const Segment<float>& a, const Capsule<float>& b, DynamicCollision& c
) {
	c = {};

	const V2_float cv{ b.segment.Direction() };
	const float mag2{ cv.Dot(cv) };

	if (NearlyEqual(mag2, 0.0f)) {
		return SegmentCircle(a, { b.segment.a, b.radius }, c);
	}

	const float mag{ std::sqrt(mag2) };
	const V2_float cu{ cv / mag };
	// Normal to b.segment
	const V2_float ncu{ cu.Skewed() };
	const V2_float ncu_dist{ ncu * b.radius };

	const Segment<float> p1{ b.segment.a + ncu_dist, b.segment.b + ncu_dist };
	const Segment<float> p2{ b.segment.a - ncu_dist, b.segment.b - ncu_dist };

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
	Segment<float> seg{ a.center, a.center + vel };

	bool start_inside{ OverlapCollision::CircleRectangle(a, b) };

	if (bool end_inside{ OverlapCollision::CircleRectangle({ seg.b, a.radius }, b) };
		start_inside && end_inside) {
		c = {};
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

	if (!OverlapCollision::SegmentRectangle(seg, e)) {
		c = {};
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
		c = {};
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
	const V2_float a_center{ a.Center() };
	return SegmentRectangle(
		{ a_center, a_center + vel }, { b.Min() - a.Half(), b.size + a.size, Origin::TopLeft }, c
	);
}

bool DynamicCollisionHandler::GeneralShape(
	const V2_float& pos1, const V2_float& size1, Origin origin1, DynamicCollisionShape shape1,
	const V2_float& pos2, const V2_float& size2, Origin origin2, DynamicCollisionShape shape2,
	const V2_float& relative_velocity, DynamicCollision& c, float& distance_squared
) {
	if (shape1 == DynamicCollisionShape::Rectangle) {
		Rectangle r1{ pos1, size1, origin1 };
		if (shape2 == DynamicCollisionShape::Rectangle) {
			Rectangle r2{ pos2, size2, origin2 };
			distance_squared = (r1.Center() - r2.Center()).MagnitudeSquared();
			return RectangleRectangle(r1, relative_velocity, r2, c);
		} else if (shape2 == DynamicCollisionShape::Circle) {
			Circle c2{ pos2, size2.x };
			distance_squared = (r1.Center() - c2.center).MagnitudeSquared();
			return CircleRectangle(c2, -relative_velocity, r1, c);
		}
		PTGN_ERROR("Unrecognized shape for collision target object");
	} else if (shape1 == DynamicCollisionShape::Circle) {
		Circle c1{ pos1, size1.x };
		if (shape2 == DynamicCollisionShape::Rectangle) {
			Rectangle r2{ pos2, size2, origin2 };
			distance_squared = (c1.center - r2.Center()).MagnitudeSquared();
			return CircleRectangle(c1, relative_velocity, r2, c);
		} else if (shape2 == DynamicCollisionShape::Circle) {
			Circle c2{ pos2, size2.x };
			distance_squared = (c1.center - c2.center).MagnitudeSquared();
			return CircleCircle(c1, -relative_velocity, c2, c);
		}
		PTGN_ERROR("Unrecognized shape for collision target object");
	}
	PTGN_ERROR("Unrecognized shape for target object");
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
			if (NearlyEqual(a.c.t, b.c.t)) {
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
	float remaining_time = 1.0f - c.t;

	switch (response) {
		case DynamicCollisionResponse::Slide: {
			V2_float tangent{ -c.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		};
		case DynamicCollisionResponse::Push: {
			return Sign(velocity.Dot(-c.normal.Skewed())) * c.normal.Swapped() * remaining_time *
				   velocity.Magnitude();
		};
		case DynamicCollisionResponse::Bounce: {
			V2_float new_velocity = velocity * remaining_time;
			if (!NearlyEqual(FastAbs(c.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(FastAbs(c.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		};
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Shutdown() {
	overlap	  = {};
	intersect = {};
	dynamic	  = {};
}

} // namespace ptgn