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

//
//template <typename T = float,
//	tt::floating_point<T> = true>
//bool CircleCapsule(const Circle<T>& a,
//				   const Capsule<T>& b) {
//	const auto n{ b.destination - b.origin };
//	const auto ap{ a.center - b.origin };
//	const T da{ ap.Dot(n) };
//	T dist2{ 0 };
//	if (da < 0) {
//		dist2 = ap.MagnitudeSquared();
//	} else {
//		const auto bp{ a.center - b.destination };
//		const T db{ bp.Dot(n) };
//		if (db < 0) {
//			const auto e{ ap - n / n.Dot(n) * da };
//			dist2 = e.MagnitudeSquared();
//		} else {
//			dist2 = bp.MagnitudeSquared();
//		}
//	}
//	const T rad{ a.radius + b.radius };
//	const T rad2{ rad * rad };
//	return dist2 < rad2 || Compare(dist2, rad2);
//}
//
//template <typename T = float,
//	tt::floating_point<T> = true>
//bool CapsuleCapsule(const Capsule<T>& a,
//					const Capsule<T>& b) {
//	math::Vector<T> c1;
//	math::Vector<T> c2;
//	T s{};
//	T t{};
//	math::ClosestPointsSegmentSegment(a, b, c1, c2, s, t);
//	const T dist2{ (c2 - c1).MagnitudeSquared() };
//	const T rad{ a.radius + b.radius };
//	const T rad2{ rad * rad };
//	const T test{ std::max(0.0f, std::sqrtf(dist2) - rad) };
//	return test < 10.0f * std::numeric_limits<float>::epsilon();
//	//return dist2 - rad2 < 10.0f * std::numeric_limits<float>::epsilon();
//	//return dist2 < rad2 || Compare(dist2, rad2);
//}


} // namespace overlap

} // namespace ptgn