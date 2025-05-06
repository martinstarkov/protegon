#include "math/geometry/circle.h"

#include <cmath>
#include <utility>
#include <vector>

#include "components/transform.h"
#include "math/math.h"
#include "math/vector2.h"
#include "common/assert.h"

namespace ptgn {

Circle::Circle(float radius) : radius{ radius } {}

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

std::vector<V2_float> Arc::GetVertices(
	const V2_float& center, bool clockwise, float sa, float ea
) const {
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

Ellipse::Ellipse(const V2_float& radius) : radius{ radius } {}

V2_float GetCenter(const Transform& transform, const Circle&) {
	return transform.position;
}

} // namespace ptgn