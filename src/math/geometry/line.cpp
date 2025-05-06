#include "math/geometry/line.h"

#include <array>

#include "components/transform.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "rendering/api/origin.h"

namespace ptgn {

Line::Line(const V2_float& start, const V2_float& end) : start{ start }, end{ end } {}

std::array<V2_float, 4> Line::GetQuadVertices(float line_width) const {
	auto dir{ end - start };
	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	auto center{ start + dir * 0.5f };
	float rotation{ dir.Angle() };
	V2_float size{ dir.Magnitude(), line_width };
	return impl::GetVertices(Transform{ center, rotation }, { size, Origin::Center });
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

Capsule::Capsule(const V2_float& start, const V2_float& end, float radius) :
	start{ start }, end{ end }, radius{ radius } {}

} // namespace ptgn