#include "math/geometry/circle.h"

#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/intersect.h"
#include "math/collision/overlap.h"
#include "math/collision/raycast.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "utility/assert.h"

namespace ptgn {

Circle::Circle(const ecs::Entity& e) : GameObject{ e } {}

Circle::Circle(const ecs::Entity& e, float radius) : Circle{ e } {
	SetRadius(radius);
}

Circle& Circle::SetRadius(float radius) {
	radius_ = radius;
	return *this;
}

float Circle::GetRadius() const {
	return radius_ * GetScale().x;
}

V2_float Circle::GetCenter() const {
	return GetPosition();
}

bool Circle::Overlaps(const V2_float& point) const {
	return point.Overlaps(*this);
}

bool Circle::Overlaps(const Line& line) const {
	return line.Overlaps(*this);
}

bool Circle::Overlaps(const Circle& circle) const {
	return OverlapCircleCircle(GetCenter(), GetRadius(), circle.GetCenter(), circle.GetRadius());
}

bool Circle::Overlaps(const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return OverlapCircleRect(GetCenter(), GetRadius(), rect_min, rect_max);
}

bool Circle::Overlaps(const Capsule& capsule) const {
	return OverlapCircleCapsule(
		GetCenter(), GetRadius(), capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

Intersection Circle::Intersects(const Circle& circle) const {
	return IntersectCircleCircle(GetCenter(), GetRadius(), circle.GetCenter(), circle.GetRadius());
}

Intersection Circle::Intersects(const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return IntersectCircleRect(GetCenter(), GetRadius(), rect_min, rect_max);
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Line& line) const {
	return RaycastCircleLine(GetCenter(), GetRadius(), ray, line.GetStart(), line.GetEnd());
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Circle& circle) const {
	return RaycastCircleCircle(
		GetCenter(), GetRadius(), ray, circle.GetCenter(), circle.GetRadius()
	);
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Capsule& capsule) const {
	return RaycastCircleCapsule(
		GetCenter(), GetRadius(), ray, capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

ptgn::Raycast Circle::Raycast(const V2_float& ray, const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return RaycastCircleRect(GetCenter(), GetRadius(), ray, rect_min, rect_max);
}

/*
void Arc::Draw(bool clockwise, const Color& color, float line_width, std::int32_t render_layer)
	const {
	PTGN_ASSERT(radius >= 0.0f, "Cannot draw filled arc with negative radius");

	// Edge case where arc is a point.
	if (NearlyEqual(radius, 0.0f)) {
		impl::Point::Draw(center.x, center.y, color.Normalized(), render_layer);
		return;
	}

	// Clamped start angle.
	float sa{ ClampAngle2Pi(start_angle) };

	// Clamped end angle.
	float ea{ ClampAngle2Pi(end_angle) };

	// Edge case where start and end angles match (considered a full rotation).
	if (float range{ sa - ea }; NearlyEqual(range, 0.0f) || NearlyEqual(range, two_pi<float>)) {
		Circle c{ center, radius };
		c.Draw(color, line_width, render_layer);
		return;
	}

	auto norm_color{ color.Normalized() };

	if (line_width == -1.0f) {
		DrawSolid(clockwise, sa, ea, norm_color, render_layer);
		return;
	}

	DrawThick(line_width, clockwise, sa, ea, norm_color, render_layer);
}

void Arc::DrawSolid(
	bool clockwise, float sa, float ea, const V4_float& color, std::int32_t render_layer
) const {
	auto vertices{ GetVertices(clockwise, sa, ea) };
	std::size_t vertex_count{ vertices.size() - 1 };
	for (std::size_t i{ 0 }; i < vertex_count; i++) {
		game.renderer.GetRenderData().AddPrimitiveTriangle(
			{ center, vertices[i], vertices[i + 1] }, render_layer, color
		);
	}
}

void Arc::DrawThick(
	float line_width, bool clockwise, float sa, float ea, const V4_float& color,
	std::int32_t render_layer
) const {
	auto vertices{ GetVertices(clockwise, sa, ea) };
	impl::DrawVertices(
		vertices.data(), vertices.size() - 1, line_width, color, render_layer, vertices.size()
	);
}
*/

Arc::Arc(const ecs::Entity& e) : GameObject{ e } {}

std::vector<V2_float> Arc::GetVertices(bool clockwise, float sa, float ea) const {
	if (sa > ea) {
		ea += two_pi<float>;
	}

	float arc_angle{ ea - sa };

	PTGN_ASSERT(arc_angle >= 0.0f);

	// Resolution indicates the number of vertices the arc is made up of. Each consecutive vertex,
	// alongside the center of the arc, makes up a triangle which is used to draw solid arcs.
	std::size_t resolution{
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * radius))
	};

	PTGN_ASSERT(
		resolution > 1, "Arc must be made up of at least two vertices (forming one triangle with "
						"the arc center point)"
	);

	float delta_angle{ arc_angle / static_cast<float>(resolution) };

	std::vector<V2_float> vertices(resolution);

	V2_float center{ GetPosition() };

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		float angle{ start_angle };
		float delta{ static_cast<float>(i) * delta_angle };
		if (clockwise) {
			angle -= delta;
		} else {
			angle += delta;
		}

		vertices[i] = center + radius * V2_float{ std::cos(angle), std::sin(angle) };
	}

	return vertices;
}

Ellipse::Ellipse(const ecs::Entity& e) : GameObject{ e } {}

Ellipse::Ellipse(const ecs::Entity& e, const V2_float& radius) : Ellipse{ e } {
	SetRadius(radius);
}

Ellipse& Ellipse::SetRadius(const V2_float& radius) {
	radius_ = radius;
	return *this;
}

V2_float Ellipse::GetRadius() const {
	return radius_ * GetScale();
}

V2_float Ellipse::GetCenter() const {
	return GetPosition();
}

} // namespace ptgn