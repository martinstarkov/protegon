#include "math/geometry/line.h"

#include <cmath>
#include <utility>

#include "collision/raycast.h"
#include "core/game.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/renderer.h"

namespace ptgn {

void Line::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.renderer.data_.AddLine(
		a, b, color.Normalized(), line_width, layer_info.z_index, layer_info.render_layer
	);
}

V2_float Line::Direction() const {
	return b - a;
}

V2_float Line::Midpoint() const {
	return (a + b) * 0.5f;
}

bool Line::Contains(const Line& line) const {
	auto d{ Direction().Cross(line.Direction()) };
	if (!NearlyEqual(d, 0.0f)) {
		return false;
	}

	float a1{ impl::ParallelogramArea(a, b, line.b) }; // Compute winding of abd (+ or -)
	float a2{ impl::ParallelogramArea(a, b, line.a) };
	bool collinear{ NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f) };

	if (!collinear) {
		return false;
	}

	if (Overlaps(line.a) && Overlaps(line.b)) {
		return true;
	}

	return false;
}

bool Line::Overlaps(const V2_float& point) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130. (SqDistPointSegment == 0) but optimized.

	V2_float ab{ Direction() };
	V2_float ac{ point - a };
	V2_float bc{ point - b };

	float e{ ac.Dot(ab) };
	// Handle cases where c projects outside ab.
	if (e < 0 || NearlyEqual(e, 0.0f)) {
		return NearlyEqual(ac.x, 0.0f) && NearlyEqual(ac.y, 0.0f);
	}

	float f{ ab.Dot(ab) };
	if (e > f || NearlyEqual(e, f)) {
		return NearlyEqual(bc.x, 0.0f) && NearlyEqual(bc.y, 0.0f);
	}

	// Handle cases where c projects onto ab.
	return NearlyEqual(ac.Dot(ac) * f, e * e);
}

bool Line::Overlaps(const Line& line) const {
	// Source:
	// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/

	// Sign of areas correspond to which side of ab points c and d are
	float a1{ impl::ParallelogramArea(a, b, line.b) }; // Compute winding of abd (+ or -)
	float a2{
		impl::ParallelogramArea(a, b, line.a)
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
		float a3{ impl::ParallelogramArea(line.a, line.b, a) }; // Compute winding of cda (+ or -)
		// Since area is constant a1 - a2 = a3 - a4, or a4 = a3 + a2 - a1
		// const T a4 = math::ParallelogramArea(c, d, b); // Must have opposite
		// sign of a3
		float a4{ a3 + a2 - a1 };
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

	[[maybe_unused]] bool point_overlap{
		(Overlaps(line.b) || Overlaps(line.a) || line.Overlaps(a) || line.Overlaps(b))
	};

	return false; // collinear && point_overlap;
}

bool Line::Overlaps(const Circle& circle) const {
	return circle.Overlaps(*this);
}

bool Line::Overlaps(const Rect& rect) const {
	return rect.Overlaps(*this);
}

bool Line::Overlaps(const Capsule& capsule) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114-115.
	float s{ 0.0f };
	float t{ 0.0f };
	V2_float c1, c2;
	return impl::WithinPerimeter(
		capsule.radius, impl::ClosestPointLineLine(*this, capsule.line, s, t, c1, c2)
	);
}

ptgn::Raycast Line::Raycast(const Line& line) const {
	// Source:
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282

	ptgn::Raycast c;

	if (!Overlaps(line)) {
		return c;
	}

	V2_float r{ Direction() };
	V2_float s{ line.Direction() };

	float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return c;
	}

	V2_float ab{ a - line.a };
	float abr{ ab.Cross(r) };

	if (float u{ abr / sr }; u < 0.0f || u > 1.0f) {
		return c;
	}

	V2_float ba{ line.a - a };
	float rs{ r.Cross(s) };
	if (NearlyEqual(rs, 0.0f)) {
		return c;
	}

	V2_float skewed{ -s.Skewed() };
	float mag2{ skewed.Dot(skewed) };
	if (NearlyEqual(mag2, 0.0f)) {
		return c;
	}

	float bas{ ba.Cross(s) };
	float t{ bas / rs };

	if (t < 0.0f || t >= 1.0f) {
		return c;
	}

	c.t		 = t;
	c.normal = skewed / std::sqrt(mag2);
	return c;
}

ptgn::Raycast Line::Raycast(const Circle& circle) const {
	// Source:
	// https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899

	ptgn::Raycast c;

	if (!Overlaps(circle)) {
		return c;
	}

	V2_float d{ -Direction() };
	V2_float f{ circle.center - a };

	// bool (roots exist), float (root 1), float (root 2).
	auto [real, t1, t2] =
		QuadraticFormula(d.Dot(d), 2.0f * f.Dot(d), f.Dot(f) - circle.radius * circle.radius);

	if (!real) {
		return c;
	}

	bool w1{ t1 >= 0.0f && t1 < 1.0f };
	bool w2{ t2 >= 0.0f && t2 < 1.0f };

	// Pick the lowest collision time that is in the [0, 1] range.
	if (w1 && w2) {
		c.t = std::min(t1, t2);
	} else if (w1) {
		c.t = t1;
	} else if (w2) {
		c.t = t2;
	} else {
		return c;
	}

	V2_float impact{ circle.center + d * c.t - a };

	float mag2{ impact.Dot(impact) };

	if (NearlyEqual(mag2, 0.0f) || NearlyEqual(mag2, circle.radius * circle.radius)) {
		c = {};
		return c;
	}

	c.normal = -impact / std::sqrt(mag2);

	return c;
}

ptgn::Raycast Line::Raycast(const Rect& rect) const {
	ptgn::Raycast c;

	bool start_in{ rect.Overlaps(a) };
	bool end_in{ rect.Overlaps(b) };

	if (start_in && end_in) {
		return c;
	}

	V2_float d{ Direction() };

	if (d.Dot(d) == 0.0f) {
		return c;
	}

	V2_float inv_dir{ 1.0f / d };

	// Calculate intersections with rectangle bounding axes.
	V2_float near{ rect.Min() - a };
	V2_float far{ rect.Max() - a };

	// Handle edge cases where the segment line is parallel with the edge of the rectangle.
	if (NearlyEqual(near.x, 0.0f)) {
		near.x = 0.0f;
	}
	if (NearlyEqual(near.y, 0.0f)) {
		near.y = 0.0f;
	}
	if (NearlyEqual(far.x, 0.0f)) {
		far.x = 0.0f;
	}
	if (NearlyEqual(far.y, 0.0f)) {
		far.y = 0.0f;
	}

	V2_float t_near{ near * inv_dir };
	V2_float t_far{ far * inv_dir };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return c;
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return c;
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x >= t_far.y || t_near.y >= t_far.x) {
		return c;
	}

	// Furthest time is contact on opposite side of target.
	// Reject if furthest time is negative, meaning the object is travelling away from the
	// target.
	float t_hit_far = std::min(t_far.x, t_far.y);
	if (t_hit_far < 0.0f) {
		return c;
	}

	if (NearlyEqual(t_near.x, t_near.y) && t_near.x == 1.0f) {
		return c;
	}

	// Closest time will be the first contact.
	bool interal{ start_in && !end_in };

	float time{ 1.0f };

	if (interal) {
		std::swap(t_near.x, t_far.x);
		std::swap(t_near.y, t_far.y);
		std::swap(inv_dir.x, inv_dir.y);
		time  = std::min(t_near.x, t_near.y);
		d	 *= -1.0f;
	} else {
		time = std::max(t_near.x, t_near.y);
	}

	if (time < 0.0f || time >= 1.0f) {
		return c;
	}

	c.t = time;

	// Contact point of collision from parametric line equation.
	// c.point = a.a + c.time * d;

	// Find which axis collides further along the movement time.

	// TODO: Figure out how to fix biasing of one direction from one side and another on the
	// other side.
	bool equal_times{ NearlyEqual(t_near.x, t_near.y) };
	bool diagonal{ NearlyEqual(FastAbs(inv_dir.x), FastAbs(inv_dir.y)) };

	if (equal_times && diagonal) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		c.normal = { -Sign(d.x), -Sign(d.y) };
	}
	if (c.normal.IsZero()) {
		if (t_near.x > t_near.y) { // X-axis.
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
	}

	if (interal) {
		std::swap(c.normal.x, c.normal.y);
		c.normal *= -1.0f;
	}

	// Raycast collision occurred.
	return c;
}

ptgn::Raycast Line::Raycast(const Capsule& capsule) const {
	// Source: https://stackoverflow.com/a/52462458

	ptgn::Raycast c;

	// TODO: Add early exit if overlap test fails.

	V2_float cv{ capsule.line.Direction() };
	float mag2{ cv.Dot(cv) };

	if (NearlyEqual(mag2, 0.0f)) {
		return Raycast(Circle{ capsule.line.a, capsule.radius });
	}

	float mag{ std::sqrt(mag2) };
	V2_float cu{ cv / mag };
	// Normal to b.line
	V2_float ncu{ cu.Skewed() };
	V2_float ncu_dist{ ncu * capsule.radius };

	Line p1{ capsule.line.a + ncu_dist, capsule.line.b + ncu_dist };
	Line p2{ capsule.line.a - ncu_dist, capsule.line.b - ncu_dist };

	ptgn::Raycast col_min{ c };
	auto c1{ Raycast(p1) };
	if (c1.Occurred() && c1.t < col_min.t) {
		col_min = c1;
	}
	auto c2{ Raycast(p2) };
	if (c2.Occurred() && c2.t < col_min.t) {
		col_min = c2;
	}
	auto c3{ Raycast(Circle{ capsule.line.a, capsule.radius }) };
	if (c3.Occurred() && c3.t < col_min.t) {
		col_min = c3;
	}
	auto c4{ Raycast(Circle{ capsule.line.b, capsule.radius }) };
	if (c4.Occurred() && c4.t < col_min.t) {
		col_min = c4;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		c = {};
		return c;
	}

	c = col_min;

	return c;
}

void Capsule::Draw(const Color& color, float line_width, const LayerInfo& layer_info) const {
	game.renderer.data_.AddCapsule(
		line.a, line.b, radius, color.Normalized(), line_width, layer_info.z_index,
		layer_info.render_layer, game.renderer.fade_
	);
}

bool Capsule::Overlaps(const V2_float& point) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114.
	return impl::WithinPerimeter(radius, impl::SquareDistancePointLine(line, point));
}

bool Capsule::Overlaps(const Line& o_line) const {
	return o_line.Overlaps(*this);
}

bool Capsule::Overlaps(const Circle& circle) const {
	return circle.Overlaps(*this);
}

bool Capsule::Overlaps(const Rect& rect) const {
	if (rect.Overlaps(line.a)) {
		return true;
	}
	if (rect.Overlaps(line.b)) {
		return true;
	}
	V2_float min{ rect.Min() };
	V2_float max{ rect.Max() };
	Line l1{ min, { max.x, min.y } };
	if (l1.Overlaps(*this)) {
		return true;
	}
	Line l2{ { max.x, min.y }, max };
	if (l2.Overlaps(*this)) {
		return true;
	}
	Line l3{ max, { min.x, max.y } };
	if (l3.Overlaps(*this)) {
		return true;
	}
	Line l4{ { min.x, max.y }, min };
	if (l4.Overlaps(*this)) {
		return true;
	}
	return false;
}

bool Capsule::Overlaps(const Capsule& capsule) const {
	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 114-115.
	float s{ 0.0f };
	float t{ 0.0f };
	V2_float c1, c2;
	return impl::WithinPerimeter(
		radius + capsule.radius, impl::ClosestPointLineLine(line, capsule.line, s, t, c1, c2)
	);
}

} // namespace ptgn
