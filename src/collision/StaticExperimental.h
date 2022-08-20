#pragma once

#include <array>
#include <limits>

#include "math/LinearAlgebraExperimental.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "physics/Types.h"
#include "utility/TypeTraits.h"

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

} // namespace intersect

} // namespace ptgn