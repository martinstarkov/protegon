#include "protegon/collision.h"

#include <cmath>
#include <limits>

#include "protegon/math.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a, const Rectangle<float>& b) {
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

float ParallelogramArea(const Point<float>& a, const Point<float>& b, const Point<float>& c) {
	return (a - c).Cross(b - c);
}

} // namespace impl

namespace overlap {

bool RectangleRectangle(const Rectangle<float>& a, const Rectangle<float>& b) {
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

bool CircleCircle(const Circle<float>& a, const Circle<float>& b) {
	const V2_float dist{ a.center - b.center };
	const float dist2{ dist.Dot(dist) };
	const float rad_sum{ a.radius + b.radius };
	const float rad_sum2{ rad_sum * rad_sum };
	return dist2 < rad_sum2 || NearlyEqual(dist2, rad_sum2);
}

bool CircleRectangle(const Circle<float>& a, const Rectangle<float>& b) {
	const float dist2{ impl::SquareDistancePointRectangle(a.center, b) };
	const float rad2{ a.radius * a.radius };
	return dist2 < rad2 || NearlyEqual(dist2, rad2);
}

bool PointRectangle(const Point<float>& a, const Rectangle<float>& b) {
	return RectangleRectangle(Rectangle<float>{ a, {}, Origin::Center }, b);
}

bool PointCircle(const Point<float>& a, const Circle<float>& b) {
	return CircleCircle({ a, 0.0f }, b);
}

bool PointSegment(const Point<float>& a, const Segment<float>& b) {
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

bool SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b) {
	const V2_float b_max{ b.Max() };
	const V2_float b_min{ b.Min() };

	Point<float> c = (b_min + b_max) * 0.5f; // Box center-point
	V2_float e	   = b_max - c;				 // Box halflength extents
	Point<float> m = (a.a + a.b) * 0.5f;	 // Segment midpoint
	V2_float d	   = a.b - m;				 // Segment halflength vector
	m			   = m - c;					 // Translate box and segment to origin
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

bool SegmentCircle(const Segment<float>& a, const Circle<float>& b) {
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
		const float triangle_area{ FastAbs(impl::ParallelogramArea(b.center, a.a, a.b)) / 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}
	return (min_dist2 < rad2 || NearlyEqual(min_dist2, rad2)) &&
		   (max_dist2 > rad2 || NearlyEqual(max_dist2, rad2));
}

bool SegmentSegment(const Segment<float>& a, const Segment<float>& b) {
	// Sign of areas correspond to which side of ab points c and d are
	const float a1{ impl::ParallelogramArea(a.a, a.b, b.b) }; // Compute winding of abd (+ or -)
	const float a2{
		impl::ParallelogramArea(a.a, a.b, b.a)
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
		const float a3{ impl::ParallelogramArea(b.a, b.b, a.a) }; // Compute winding of cda (+ or -)
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

} // namespace overlap

namespace intersect {

bool CircleCircle(const Circle<float>& a, const Circle<float>& b, Collision& c) {
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

bool RectangleRectangle(const Rectangle<float>& a, const Rectangle<float>& b, Collision& c) {
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

bool CircleRectangle(const Circle<float>& a, const Rectangle<float>& b, Collision& c) {
	c = {};

	const V2_float half{ b.Half() };
	const V2_float b_max{ b.Max() };
	const V2_float b_min{ b.Min() };
	const V2_float clamped{ std::clamp(a.center.x, b_min.x, b_max.x),
							std::clamp(a.center.y, b_min.y, b_max.y) };
	const V2_float ab{ a.center - clamped };

	const float d2{ ab.Dot(ab) };
	const float r2{ a.radius * a.radius };

	if (d2 < r2) {
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

} // namespace intersect

namespace dynamic {

bool SegmentSegment(const Segment<float>& a, const Segment<float>& b, Collision& c) {
	c = {};

	const Point<float> r{ a.Direction() };
	const Point<float> s{ b.Direction() };

	const float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return false;
	}

	const Point<float> ab{ a.a - b.a };
	const float abr{ ab.Cross(r) };

	const float u{ abr / sr };
	if (u < 0.0f || u > 1.0f) {
		return false;
	}

	const Point<float> ba{ b.a - a.a };
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

bool SegmentCircle(const Segment<float>& a, const Circle<float>& b, Collision& c) {
	c = {};

	const Point<float> d{ -a.Direction() };
	const Point<float> f{ b.center - a.a };

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

bool SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b, Collision& c) {
	c = {};

	const V2_float d{ a.Direction() };

	if (NearlyEqual(d.Dot(d), 0.0f)) {
		return false;
	}

	const V2_float b_min{ b.Min() };
	const V2_float b_max{ b.Max() };
	const V2_float inv{ 1.0f / d.x, 1.0f / d.y };
	const V2_float d0{ (b_min - a.a) * inv };
	const V2_float d1{ (b_max - a.a) * inv };
	const V2_float v0{ std::min(d0.x, d1.x), std::min(d0.y, d1.y) };
	const V2_float v1{ std::max(d0.x, d1.x), std::max(d0.y, d1.y) };

	const float lo{ std::max(v0.x, v0.y) };
	const float hi{ std::min(v1.x, v1.y) };

	if (hi >= 0.0f && hi >= lo && lo <= 1.0f) {
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

		if (abs_coeff.x > abs_coeff.y) {
			c.normal = { Sign(coeff.x), 0.0f };
		} else {
			c.normal = { 0.0f, Sign(coeff.y) };
		}

		return true;
	}
	return false;
}

bool SegmentCapsule(const Segment<float>& a, const Capsule<float>& b, Collision& c) {
	c = {};

	const Point<float> cv{ b.segment.Direction() };
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

	Collision col_min{ c };
	if (SegmentSegment(a, p1, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	if (SegmentSegment(a, p2, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	if (SegmentCircle(a, { b.segment.a, b.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	if (SegmentCircle(a, { b.segment.b, b.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		c = {};
		return false;
	}

	c = col_min;
	return true;
}

bool CircleCircle(
	const Circle<float>& a, const Vector2<float>& vel, const Circle<float>& b, Collision& c
) {
	return SegmentCircle({ a.center, a.center + vel }, { b.center, b.radius + a.radius }, c);
}

bool CircleRectangle(
	const Circle<float>& a, const Vector2<float>& vel, const Rectangle<float>& b, Collision& c
) {
	Segment<float> seg{ a.center, a.center + vel };

	bool start_inside{ overlap::CircleRectangle(a, b) };
	bool end_inside{ overlap::CircleRectangle({ seg.b, a.radius }, b) };

	if (start_inside && end_inside) {
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

	if (!overlap::SegmentRectangle(seg, e)) {
		c = {};
		return false;
	}

	V2_float b_min{ b.Min() };
	V2_float b_max{ b.Max() };

	Collision col_min{ c };
	// Top segment.
	if (SegmentCapsule(seg, { { b_min, V2_float{ b_max.x, b_min.y } }, a.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	// Right segment.
	if (SegmentCapsule(seg, { { V2_float{ b_max.x, b_min.y }, b_max }, a.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	// Bottom segment.
	if (SegmentCapsule(seg, { { b_max, V2_float{ b_min.x, b_max.y } }, a.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
	}
	// Left segment.
	if (SegmentCapsule(seg, { { V2_float{ b_min.x, b_max.y }, b_min }, a.radius }, c)) {
		if (c.t < col_min.t) {
			col_min = c;
		}
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

bool RectangleRectangle(
	const Rectangle<float>& a, const Vector2<float>& vel, const Rectangle<float>& b, Collision& c
) {
	const V2_float a_center{ a.Center() };
	return SegmentRectangle(
		{ a_center, a_center + vel }, { b.Min() - a.Half(), b.size + a.size, Origin::TopLeft }, c
	);
}

} // namespace dynamic

} // namespace ptgn