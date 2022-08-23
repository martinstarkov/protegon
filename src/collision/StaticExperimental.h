#pragma once

#include <array>
#include <limits>

#include "math/LinearAlgebraExperimental.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "physics/Types.h"
#include "utility/TypeTraits.h"
#include "collision/OverlapExperimental.h"

namespace ptgn {

namespace intersect {

struct Collision {
	float depth{ 0.0f };
	V2_float normal{ 0.0f, 0.0f };
	void Reset() {
		depth = 0.0f;
		normal = { 0.0f, 0.0f };
	}
};

bool CircleCircle(const Circle<float>& A,
				  const Circle<float>& B,
				  Collision& c) {
	c.Reset();

	const V2_float d{ B.c - A.c };
	const float dist2{ Dot(d, d) };
	const float r{ A.r + B.r };
	
	if (dist2 > r * r)
		return false;

	// Edge case where circle centers are in the same location.
	c.normal = { 1.0f, 0.0f };
	c.depth = r;
	
	if (dist2 > epsilon2<float>) {
		const float dist{ std::sqrtf(dist2) };
		c.normal = -d / dist;
		c.depth = r - dist;
	}
	return true;
}

bool AABBAABB(const AABB<float>& A,
              const AABB<float>& B,
              Collision& c) {
    c.Reset();

	const V2_float A_h{ A.Half() };
	const V2_float B_h{ B.Half() };
	const V2_float d{ B.p + B_h - (A.p + A_h) };
	const V2_float pen{ A_h + B_h - FastAbs(d) };

    if (pen.x < 0 || pen.y < 0 || NearlyEqual(pen.x, 0.0f) || NearlyEqual(pen.y, 0.0f))
        return false;

	if (NearlyEqual(d.x, 0.0f) && NearlyEqual(d.y, 0.0f)) {
		// Edge case where aabb centers are in the same location.
		c.normal = { 1.0f, 0.0f };
		c.depth = A_h.x + B_h.x;
	} else if (pen.y < pen.x) {
		c.normal = { 0.0f, -Sign(d.y) };
		c.depth = FastAbs(pen.y);
    } else {
		c.normal = { -Sign(d.x), 0.0f };
		c.depth = FastAbs(pen.x);
    }
    return true;
}

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
bool CircleAABB(const Circle<float>& A,
				const AABB<float>& B,
				Collision& c) {
	using Edge = std::pair<V2_float, V2_float>;

	V2_float top_right{ B.p.x + B.s.x, B.p.y };
	V2_float bottom_right{ B.p + B.s };
	V2_float bottom_left{ B.p.x, B.p.y + B.s.y };
	
	std::array<Edge, 4> edges;
	edges.at(0) = { B.p,          top_right };    // top
	edges.at(1) = { top_right,    bottom_right }; // right
	edges.at(2) = { bottom_right, bottom_left };  // bottom
	edges.at(3) = { bottom_left,  B.p };          // left
	
	float min_dist2{ std::numeric_limits<float>::infinity() };
	V2_float min_point;
	std::size_t side{ 0 };
	
	for (std::size_t i{ 0 }; i < edges.size(); ++i) {
		auto& [a, b] = edges[i];
		float t{};
		V2_float c1;
		math::ClosestPointSegment(A.c, { a, b }, t, c1);
		const V2_float d{ A.c - c1 };
		float dist2{ Dot(d, d) };
		if (dist2 < min_dist2) {
			side = i;
			min_dist2 = dist2;
			// Point on the AABB that was the closest.
			min_point = c1;
		}
	}

	bool inside{ overlap::PointAABB(A.c, B) };

	if (!inside && min_dist2 > A.r * A.r)
		return false;

	if (NearlyEqual(min_dist2, 0.0f)) {
		// Circle is on one of the AABB edges.
		switch (side) {
			case 0:
				c.normal = { 0.0f, -1.0f }; // top
				break;
			case 1:
				c.normal = { 1.0f, 0.0f };  // right
				break;
			case 2:
				c.normal = { 0.0f, 1.0f };  // bottom
				break;
			case 3:
				c.normal = { -1.0f, 0.0f }; // left
				break;
		}
		c.depth = A.r;
	} else {
		const V2_float d{ A.c - min_point };
		const float mag2{ Dot(d, d) };

		if (NearlyEqual(mag2, 0.0f)) {
			// Choose upward vector arbitrarily if circle center is aabb center.
			c.normal = { 0, -1 }; // top
			c.depth = A.r;
		} else {
			const float mag{ std::sqrtf(mag2) };
			c.normal = d / mag;
			if (inside) {
				c.normal *= -1;
				c.depth = A.r + mag;
			} else {
				c.depth = A.r - mag;
			}
		}

	}
	return true;
}

bool CircleCapsule(const Circle<float>& A,
				   const Capsule<float>& B,
				   Collision& c) {
	const V2_float ab{ B.Direction() };
	if (ab.IsZero())
		return CircleCircle(A, { B.a, B.r }, c);
	V2_float p;
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	const float t{ (A.c - B.a).Dot(ab) };
	const float denom{ ab.MagnitudeSquared() }; // Always nonnegative since denom = ||ab||^2
	if (t > 0) {
		if (t < denom) {
			// c projects inside the [a,b] interval; must do deferred divide now
			p = B.a + t / denom * ab;
		} else {
			// c projects outside the [a,b] interval, on the b side; clamp to b
			p = B.b;
		}
	} else {
		// c projects outside the [a,b] interval, on the a side; clamp to a
		p = B.a;
	}
	const auto d{ p - A.c };
	const float dist2{ Dot(d, d) };
	const float r{ A.r + B.r };

	if (dist2 > r * r)
		return false;

	if (NearlyEqual(dist2, 0.0f)) {
		assert(!NearlyEqual(denom, 0.0f));
		c.normal = ab.Skewed() / std::sqrtf(denom);
		c.depth = r;
	} else {
		const float dist{ std::sqrtf(dist2) };
		c.normal = -d / dist;
		c.depth = r - dist;
	}
	return true;
}

bool CapsuleCapsule(const Capsule<float>& A,
					const Capsule<float>& B,
					Collision& c) {
	V2_float c1;
	V2_float c2;
	float s{ 0 };
	float t{ 0 };
	math::ClosestPointsSegmentSegment(A, B, c1, c2, s, t);
	const auto dir{ c2 - c1 };
	const float dist2{ dir.MagnitudeSquared() };
	const float r{ A.r + B.r };
	
	if (dist2 > r * r)
		return false;

	if (!NearlyEqual(dist2, 0.0f)) {
		float dist{ std::sqrtf(dist2) };
		c.normal = -dir / dist;
		c.depth = r - dist;
	} else {
		const float mag_a2{ A.Direction().MagnitudeSquared() }; // Squared length of segment S1, always nonnegative
		const float mag_b2{ B.Direction().MagnitudeSquared() }; // Squared length of segment S2, always nonnegative
		// Check if either or both segments degenerate into points
		bool a_point{ NearlyEqual(mag_a2, 0.0f) };
		bool b_point{ NearlyEqual(mag_b2, 0.0f) };
		if (a_point && b_point) {
			return CircleCircle({ A.a, A.r }, { B.a, B.r }, c);
		} else if (a_point) {
			return CircleCapsule({ A.a, A.r }, B, c);
		} else if (b_point) {
			bool occured{ CircleCapsule({ B.a, B.r }, A, c) };
			c.normal *= -1;
			return occured;
		}

		// Capsules lines intersect, different kind of routine needed.
		const float mag_a{ std::sqrtf(mag_a2) };
		const float mag_b{ std::sqrtf(mag_b2) };
		const std::array<float, 4> f{ s * mag_a, (1 - s) * mag_a, t * mag_b, (1 - t) * mag_b };
		const std::array<V2_float, 4> ep{ A.a, A.b, B.a, B.b };
		// Determine which end of both capsules is closest to intersection point.
		const auto min_i{ std::distance(std::begin(f), std::min_element(std::begin(f), std::end(f))) };
		const auto half{ min_i / 2 };
		const auto sign{ 1 - 2 * half };
		// This code replaces the 4 if-statements below but is less readable.
		const auto max_i{ half < 1 ? (min_i + 1) % 2 : (min_i - 1) % 2 + 2 };
		float min_dist2 = f[min_i];
		Line<float> line{ A.a, A.b };
		Line<float> other{ B.a, B.b };
		if (half > 0) {
			Swap(line.a, other.a);
			Swap(line.b, other.b);
		}
		// Capsule vs capsule.
		float frac{}; // frac is an unused variable.
		Point<float> point;
		// TODO: Fix this awful branching.
		math::ClosestPointLine(ep[min_i], other, frac, point);
		const V2_float to_min{ ep[min_i] - point };
		if (!to_min.IsZero()) {
			// Capsule centerlines intersect each other.
			c.normal = -sign * to_min.Normalized();
			c.depth = to_min.Magnitude() + r;
		} else {
			c.depth = r;
			// Capsule centerlines touch in at least one location.
			math::ClosestPointLine(ep[max_i], other, frac, point);
			const V2_float to_max{ point - ep[max_i] };
			if (!to_max.IsZero()) { // Capsule a or b lies on the other capsule's centerline.
				c.normal = -sign * to_max.Normalized();
			} else if (DistanceSquared(ep[min_i], point) > 0) { // Push capsules apart in perpendicular direction.
				// Capsules are collinear.
				c.normal = -line.Direction().Skewed().Normalized();
			} else { // Push capsules apart in parallel direction.
				c.normal = -line.Direction().Normalized();
			}
		}
	}
	return true;
}

} // namespace intersect

} // namespace ptgn