#pragma once

#include <algorithm> // std::clamp

#include "math/Vector2.h"
#include "utility/TypeTraits.h"
#include "physics/Types.h"

namespace ptgn {

namespace math {

// Computes closest points out_c1 and out_c2 of
// S1(out_s) = a.origin + out_s * (A.b - A.a)
// S2(out_t) = b.origin + out_t * (B.b - B.a)
template <typename T = float,
	tt::floating_point<T> = true>
void ClosestPointsSegmentSegment(const Segment<T>& A,
								 const Segment<T>& B,
								 Point<T>& out_c1,
								 Point<T>& out_c2,
								 T& out_s,
								 T& out_t) {
	const Vector2<T> d1{ A.Direction() };
	const Vector2<T> d2{ B.Direction() };
	const Vector2<T> r{ A.a - B.a };
	const T mag_a2{ d1.MagnitudeSquared() }; // Squared length of segment S1, always nonnegative
	const T mag_b2{ d2.MagnitudeSquared() }; // Squared length of segment S2, always nonnegative
	// Check if either or both segments degenerate into points
	bool a_point{ NearlyEqual(mag_a2, 0.0f) };
	bool b_point{ NearlyEqual(mag_b2, 0.0f) };
	if (a_point && b_point) {
		// Both segments degenerate into points.
		out_s = out_t = 0.0f;
		out_c1 = A.a;
		out_c2 = B.a;
		return;
	} else if (a_point) {
		// First segment degenerates into a point.
		const T bdr{ d2.Dot(r) };
		out_s = 0.0f;
		out_t = bdr / mag_b2; // out_s = 0 => out_t = (B * out_s + bdr) / mag_b2 = bdr / mag_b2
		out_t = std::clamp(out_t, 0.0f, 1.0f);
	} else if (b_point) {
		// Second segment degenerates into a point.
		const T adr{ d1.Dot(r) };
		out_t = 0.0f;
		out_s = std::clamp(-adr / mag_a2, 0.0f, 1.0f); // out_t = 0 => out_s = (B * out_t - adr) / mag_a2 = -adr / mag_a2
	} else {
		const T adr{ d1.Dot(r) };
		const T bdr{ d2.Dot(r) };
		// The general non-degenerate case starts here.
		const T adb{ d1.Dot(d2) };
		const T denom{ mag_a2 * mag_b2 - adb * adb }; // Always nonnegative
		// If segments not parallel, compute closest point on L1 to L2 and
		// clamp to segment S1. Else pick arbitrary s (here 0)
		if (NearlyEqual(denom, 0.0f))
			out_s = 0.0f;
		else 
			out_s = std::clamp((adb * bdr - adr * mag_b2) / denom, 0.0f, 1.0f);
		const T tnom{ adb * out_s + bdr };
		if (tnom < 0.0f) {
			out_t = 0.0f;
			out_s = std::clamp(-adr / mag_a2, 0.0f, 1.0f);
		} else if (tnom > mag_b2) {
			out_t = 1.0f;
			out_s = std::clamp((adb - adr) / mag_a2, 0.0f, 1.0f);
		} else {
			out_t = tnom / mag_b2;
		}
	}
	out_c1 = A.a + d1 * out_s;
	out_c2 = B.a + d2 * out_t;
}

// Given an infinite line line_origin->line_destination and point, computes closest point out_d on ab.
// Also returns out_t for the parametric position of out_d, out_d(t)= A + out_t * (B - A)
template <typename T = float,
	tt::floating_point<T> = true>
void ClosestPointLine(const Point<T>& A,
					  const Line<T>& B,
					  T& out_t,
					  Point<T>& out_d) {
	Vector2<T> d{ B.Direction() };
	// Project c onto ab, but deferring divide by Dot(ab, ab)
	out_t = Dot(A - B.a, d) / Dot(d, d);
	out_d = B.a + out_t * d;
}

template <typename T = float,
	tt::floating_point<T> = true>
void ClosestPointSegment(const Point<T>& A,
					     const Segment<T>& B,
					     T& out_t,
					     Point<T>& out_d) {
	Vector2<T> ab{ B.Direction() };
	// Project A onto ab, but deferring divide by Dot(ab, ab)
	out_t = Dot(A - B.a, ab);
	if (out_t <= 0.0f) {
		// A projects outside the [B.a,B.b] interval, on the B.a side; clamp to B.a
		out_t = 0.0f;
		out_d = B.a;
	} else {
		const T denom{ Dot(ab, ab) }; // Always nonnegative since denom = ||ab||^2
		if (out_t >= denom) {
			// A projects outside the [B.a,B.b] interval, on the B.b side; clamp to B.b
			out_t = 1.0f;
			out_d = B.b;
		} else {
			// A projects inside the [B.a,B.b] interval; must do deferred divide now
			out_t = out_t / denom;
			out_d = B.a + out_t * ab;
		}
	}
}

} // namespace math

} // namespace ptgn