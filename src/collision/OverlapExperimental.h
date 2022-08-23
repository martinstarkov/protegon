#pragma once

#include "math/LinearAlgebraExperimental.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "physics/Types.h"
#include "utility/TypeTraits.h"

namespace ptgn {


namespace overlap {

bool CircleCircle(const Circle<float>& A,
				  const Circle<float>& B) {
	const V2_float d{ B.c - A.c };
	const float r{ A.r + B.r };
	return Dot(d, d) <= r * r;
}

bool AABBAABB(const AABB<float>& A,
			  const AABB<float>& B) {
	if (A.p.x + A.s.x < B.p.x || A.p.x > B.p.x + B.s.x)
		return false;
	if (A.p.y + A.s.y < B.p.y || A.p.y > B.p.y + B.s.y)
		return false;
	return true;
}

bool CircleAABB(const Circle<float>& A,
				const AABB<float>& B) {
	const V2_float c{ Clamp(A.c, B.Min(), B.Max()) };
	const V2_float d{ A.c - c };
	return Dot(d, d) <= A.r * A.r;
}

bool PointAABB(const Point<float>& A,
			  const AABB<float>& B) {
	return AABBAABB({ A, { 0.0f, 0.0f } }, B);
}

bool CircleCapsule(const Circle<float>& A,
				   const Capsule<float>& B) {
	const V2_float n{ B.Direction() };
	const V2_float ap{ A.c - B.a };
	const float da{ ap.Dot(n) };
	float dist2{ 0.0f };
	if (da < 0.0f) {
		dist2 = Dot(ap, ap);
	} else {
		const V2_float bp{ A.c - B.b };
		const float db{ bp.Dot(n) };
		if (db < 0.0f) {
			const V2_float e{ ap - n / n.Dot(n) * da };
			dist2 = Dot(e, e);
		} else {
			dist2 = Dot(bp, bp);
		}
	}
	const float r{ A.r + B.r };
	return dist2 <= r * r;
}

bool CapsuleCapsule(const Capsule<float>& A,
					const Capsule<float>& B) {
	V2_float c1;
	V2_float c2;
	float s{};
	float t{};
	math::ClosestPointsSegmentSegment(A, B, c1, c2, s, t);
	const float r{ A.r + B.r };
	return (c2 - c1).MagnitudeSquared() <= r * r;
}

} // namespace overlap

} // namespace ptgn