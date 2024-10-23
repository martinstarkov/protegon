#include "math/geometry/circle.h"

#include <cmath>
#include <limits>
#include <utility>

#include "algorithm"
#include "math/geometry/intersection.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "utility/debug.h"

namespace ptgn {

bool Circle::Overlaps(const V2_float& point) const {
	V2_float dist{ center - point };
	return WithinPerimeter(radius, dist.Dot(dist));
}

bool Circle::Overlaps(const Circle& circle) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 88.
	V2_float dist{ center - circle.center };
	return WithinPerimeter(radius + circle.radius, dist.Dot(dist));
}

bool Circle::Overlaps(const Rect& rect) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	return WithinPerimeter(radius, impl::SquareDistancePointRect(center, rect));
}

bool Circle::Overlaps(const Line& line) const {
	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection

	// If the line is inside the circle entirely, exit early.
	if (Overlaps(line.a) && Overlaps(line.b)) {
		return true;
	}

	float min_dist2{ std::numeric_limits<float>::infinity() };

	// O is the circle center, P is the line origin, Q is the line destination.
	V2_float OP{ line.a - center };
	V2_float OQ{ line.b - center };
	V2_float PQ{ line.Direction() };

	float OP_dist2{ OP.Dot(OP) };
	float OQ_dist2{ OQ.Dot(OQ) };
	float max_dist2{ std::max(OP_dist2, OQ_dist2) };

	if (OP.Dot(-PQ) > 0.0f && OQ.Dot(PQ) > 0.0f) {
		float triangle_area{ FastAbs(impl::ParallelogramArea(center, line.a, line.b)) / 2.0f };
		min_dist2 = 4.0f * triangle_area * triangle_area / PQ.Dot(PQ);
	} else {
		min_dist2 = std::min(OP_dist2, OQ_dist2);
	}

	return WithinPerimeter(radius, min_dist2) && !WithinPerimeter(radius, max_dist2);
}

bool Circle::WithinPerimeter(float radius, float dist2) {
	float rad2{ radius * radius };

	if (dist2 < rad2) {
		return true;
	}

	// Optional: Include perimeter:
	/*if (NearlyEqual(dist2, rad2)) {
		return true;
	}*/

	return false;
}

Intersection Circle::Intersects(const Circle& circle) const {
	Intersection c;

	V2_float d{ circle.center - center };
	float dist2{ d.Dot(d) };
	float r{ radius + circle.radius };

	// No overlap.
	if (!WithinPerimeter(r, dist2)) {
		return c;
	}

	if (dist2 > epsilon2<float>) {
		float dist{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(dist, 0.0f));
		c.normal = -d / dist;
		c.depth	 = r - dist;
	} else {
		// Edge case where circle centers are in the same location.
		c.normal.y = -1.0f; // default to upward normal.
		c.depth	   = r;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

Intersection Circle::Intersects(const Rect& rect) const {
	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	Intersection c;

	V2_float half{ rect.Half() };
	V2_float max{ rect.Max() };
	V2_float min{ rect.Min() };
	V2_float clamped{ std::clamp(center.x, min.x, max.x), std::clamp(center.y, min.y, max.y) };
	V2_float ab{ center - clamped };

	float dist2{ ab.Dot(ab) };

	// No overlap.
	if (!WithinPerimeter(radius, dist2)) {
		return c;
	}

	if (!NearlyEqual(dist2, 0.0f)) {
		// Shallow intersection (center of circle not inside of AABB).
		float d{ std::sqrt(dist2) };
		PTGN_ASSERT(!NearlyEqual(d, 0.0f));
		c.normal = ab / d;
		c.depth	 = radius - d;
		PTGN_ASSERT(c.depth >= 0.0f);
		return c;
	}

	// Deep intersection (center of circle inside of AABB).

	// Clamp circle's center to edge of AABB, then form the manifold.
	V2_float mid{ rect.Center() };
	V2_float d{ mid - center };

	V2_float overlap{ half - V2_float{ FastAbs(d.x), FastAbs(d.y) } };

	if (overlap.x < overlap.y) {
		c.depth	   = radius + overlap.x;
		c.normal.x = d.x < 0 ? 1.0f : -1.0f;
	} else {
		c.depth	   = radius + overlap.y;
		c.normal.y = d.y < 0 ? 1.0f : -1.0f;
	}

	PTGN_ASSERT(c.depth >= 0.0f);

	return c;
}

} // namespace ptgn