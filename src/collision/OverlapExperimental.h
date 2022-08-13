#pragma once

#include "math/LinearAlgebraExperimental.h"
#include "math/Math.h"
#include "math/Vector2.h"
#include "physics/Types.h"
#include "utility/TypeTraits.h"

namespace ptgn {

namespace overlap {

template <typename T = float,
	tt::floating_point<T> = true>
bool CircleCircle(const Circle<T>& a,
				  const Circle<T>& b) {
	const auto dir{ b.center - a.center };
	const T rad{ a.radius + b.radius };
	return dir.MagnitudeSquared() < rad * rad;
}

template <typename T = float,
	tt::floating_point<T> = true>
bool AABBAABB(const AABB<T>& a,
			  const AABB<T>& b) {
	if (a.position.x + a.size.x < b.position.x ||
		a.position.x > b.position.x + b.size.x)
		return false;
	if (a.position.y + a.size.y < b.position.y ||
		a.position.y > b.position.y + b.size.y)
		return false;
	return true;
}

template <typename T = float,
	tt::floating_point<T> = true>
bool CircleAABB(const Circle<T>& a,
				const AABB<T>& b) {
	const auto clamped{ math::Clamp(a.center, b.Min(), b.Max()) };
	const auto dir{ a.center - clamped };
	const T dist2{ dir.MagnitudeSquared() };
	const T rad2{ a.RadiusSquared() };
	return dist2 < rad2;
}

template <typename T = float,
	tt::floating_point<T> = true>
bool CircleCapsule(const Circle<T>& a,
				   const Capsule<T>& b) {
	const auto n{ b.destination - b.origin };
	const auto ap{ a.center - b.origin };
	const T da{ ap.Dot(n) };
	T dist2{ 0 };
	if (da < 0) {
		dist2 = ap.MagnitudeSquared();
	} else {
		const auto bp{ a.center - b.destination };
		const T db{ bp.Dot(n) };
		if (db < 0) {
			const auto e{ ap - n / n.Dot(n) * da };
			dist2 = e.MagnitudeSquared();
		} else {
			dist2 = bp.MagnitudeSquared();
		}
	}
	const T rad{ a.radius + b.radius };
	return dist2 < rad * rad;
}

template <typename T = float,
	tt::floating_point<T> = true>
bool CapsuleCapsule(const Capsule<T>& a,
					const Capsule<T>& b) {
	math::Vector2<T> c1;
	math::Vector2<T> c2;
	math::ClosestPointsSegmentSegment(a, b, c1, c2);
	const T dist2{ (c2 - c1).MagnitudeSquared() };
	const T rad{ a.radius + b.radius };
	return dist2 < rad * rad;
}


} // namespace overlap

} // namespace ptgn