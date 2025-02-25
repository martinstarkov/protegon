#include "math/geometry/line.h"

#include <array>

#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/overlap.h"
#include "math/collision/raycast.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"

namespace ptgn {

Line::Line(const ecs::Entity& e) : GameObject{ e } {}

Line::Line(const ecs::Entity& e, const V2_float& start, const V2_float& end) : Line{ e } {
	SetVertices(start, end);
}

Line& Line::SetStart(const V2_float& start) {
	SetPosition(start);
	return *this;
}

Line& Line::SetEnd(const V2_float& end) {
	end_ = end;
	return *this;
}

Line& Line::SetVertices(const V2_float& start, const V2_float& end) {
	SetStart(start);
	SetEnd(end);
	return *this;
}

std::array<V2_float, 2> Line::GetVertices() const {
	return { GetStart(), end_ };
}

V2_float Line::GetStart() const {
	return GetPosition();
}

V2_float Line::GetEnd() const {
	return end_;
}

bool Line::Contains(const Line& line) const {
	auto [start, end]			= GetVertices();
	auto [line_start, line_end] = line.GetVertices();

	if (auto d{ (end - start).Cross(line_end - line_start) }; !NearlyEqual(d, 0.0f)) {
		return false;
	}

	float a1{ impl::ParallelogramArea(start, end, line_end) }; // Compute winding of abd (+ or -)
	float a2{ impl::ParallelogramArea(start, end, line_start) };

	if (bool collinear{ NearlyEqual(a1, 0.0f) || NearlyEqual(a2, 0.0f) }; !collinear) {
		return false;
	}

	if (Overlaps(line_start) && Overlaps(line_end)) {
		return true;
	}

	return false;
}

bool Line::Overlaps(const V2_float& point) const {
	return point.Overlaps(*this);
}

bool Line::Overlaps(const Line& line) const {
	return OverlapLineLine(GetStart(), GetEnd(), line.GetStart(), line.GetEnd());
}

bool Line::Overlaps(const Circle& circle) const {
	return OverlapLineCircle(GetStart(), GetEnd(), circle.GetCenter(), circle.GetRadius());
}

bool Line::Overlaps(const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return OverlapLineRect(GetStart(), GetEnd(), rect_min, rect_max);
}

bool Line::Overlaps(const Capsule& capsule) const {
	return OverlapLineCapsule(
		GetStart(), GetEnd(), capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

ptgn::Raycast Line::Raycast(const Line& line) const {
	return RaycastLineLine(GetStart(), GetEnd(), line.GetStart(), line.GetEnd());
}

ptgn::Raycast Line::Raycast(const Circle& circle) const {
	return RaycastLineCircle(GetStart(), GetEnd(), circle.GetCenter(), circle.GetRadius());
}

ptgn::Raycast Line::Raycast(const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return RaycastLineRect(GetStart(), GetEnd(), rect_min, rect_max);
}

ptgn::Raycast Line::Raycast(const Capsule& capsule) const {
	return RaycastLineCapsule(
		GetStart(), GetEnd(), capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

std::array<V2_float, 4> Line::GetQuadVertices(float line_width) const {
	auto [start, end] = GetVertices();
	return GetQuadVertices(start, end, line_width);
}

std::array<V2_float, 4> Line::GetQuadVertices(
	const V2_float& start, const V2_float& end, float line_width
) {
	V2_float dir{ end - start };
	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	V2_float center{ start + dir * 0.5f };
	float rotation{ dir.Angle() };
	V2_float size{ dir.Magnitude(), line_width };
	return impl::GetQuadVertices(center, rotation, size, V2_float{ 0.5f, 0.5f });
}

/*
void Capsule::Draw(const Color& color, float line_width, std::int32_t render_layer) const {
	V2_float dir{ line.Direction() };
	float dir2{ dir.Dot(dir) };

	// Edge case where capsule has no length, i.e. it is a circle.
	if (NearlyEqual(dir2, 0.0f)) {
		Circle c{ line.a, radius };
		c.Draw(color, line_width, render_layer);
		return;
	}

	float angle{ dir.Angle() + half_pi<float> };
	float start_angle{ angle };
	float end_angle{ angle };

	auto norm_color{ color.Normalized() };

	Arc a1{ line.a, radius, start_angle, end_angle + pi<float> };
	Arc a2{ line.b, radius, start_angle + pi<float>, end_angle };

	if (line_width == -1.0f) {
		line.DrawThick(radius * 2.0f, norm_color, render_layer);

		// How many radians into the line the arc protrudes.
		constexpr float delta{ DegToRad(0.5f) };
		a1.start_angle -= delta;
		a1.end_angle   += delta;
		a2.start_angle -= delta;
		a2.end_angle   += delta;

		a1.DrawSolid(
			false, ClampAngle2Pi(a1.start_angle), ClampAngle2Pi(a1.end_angle), norm_color,
			render_layer
		);
		a2.DrawSolid(
			false, ClampAngle2Pi(a2.start_angle), ClampAngle2Pi(a2.end_angle), norm_color,
			render_layer
		);
		return;
	}

	V2_float tangent_r{ Floor(dir.Skewed() / std::sqrt(dir2) * radius) };

	Line l1{ line.a + tangent_r, line.b + tangent_r };
	Line l2{ line.a - tangent_r, line.b - tangent_r };

	l1.DrawThick(line_width, norm_color, render_layer);
	l2.DrawThick(line_width, norm_color, render_layer);

	a1.DrawThick(
		line_width, false, ClampAngle2Pi(a1.start_angle), ClampAngle2Pi(a1.end_angle), norm_color,
		render_layer
	);
	a2.DrawThick(
		line_width, false, ClampAngle2Pi(a2.start_angle), ClampAngle2Pi(a2.end_angle), norm_color,
		render_layer
	);
}
*/

Capsule::Capsule(const ecs::Entity& e) : GameObject{ e } {}

Capsule::Capsule(const ecs::Entity& e, const V2_float& start, const V2_float& end, float radius) :
	Capsule{ e } {
	SetVertices(start, end);
	SetRadius(radius);
}

Capsule& Capsule::SetStart(const V2_float& start) {
	SetPosition(start);
	return *this;
}

Capsule& Capsule::SetEnd(const V2_float& end) {
	end_ = end;
	return *this;
}

Capsule& Capsule::SetRadius(float radius) {
	radius_ = radius;
	return *this;
}

Capsule& Capsule::SetVertices(const V2_float& start, const V2_float& end) {
	SetStart(start);
	SetEnd(end);
	return *this;
}

std::array<V2_float, 2> Capsule::GetVertices() const {
	return { GetStart(), end_ };
}

V2_float Capsule::GetStart() const {
	return GetPosition();
}

V2_float Capsule::GetEnd() const {
	return end_;
}

float Capsule::GetRadius() const {
	return radius_;
}

bool Capsule::Overlaps(const V2_float& point) const {
	return point.Overlaps(*this);
}

bool Capsule::Overlaps(const Line& line) const {
	return line.Overlaps(*this);
}

bool Capsule::Overlaps(const Circle& circle) const {
	return circle.Overlaps(*this);
}

bool Capsule::Overlaps(const Rect& rect) const {
	return rect.Overlaps(*this);
}

bool Capsule::Overlaps(const Capsule& capsule) const {
	return OverlapCapsuleCapsule(
		GetStart(), GetEnd(), radius_, capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

} // namespace ptgn