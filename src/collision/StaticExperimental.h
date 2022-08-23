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
    //Vector2<T> point[2];
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

	const Vector2<float> A_h{ A.Half() };
	const Vector2<float> B_h{ B.Half() };
	const Vector2<float> d{ B.p + B_h - (A.p + A_h) };
	const Vector2<float> pen{ A_h + B_h - FastAbs(d) };

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

	const float rad2{ A.r * A.r };
	if (!inside && min_dist2 > rad2)
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

} // namespace intersect

} // namespace ptgn