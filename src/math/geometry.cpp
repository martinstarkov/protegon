#include "math/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "geometry/rect.h"
#include "math/vector2.h"

namespace ptgn {

Transform ApplyOffset(const Shape& shape, const Transform& transform, const Entity& entity) {
	if (!std::holds_alternative<Rect>(shape)) {
		return transform;
	}
	const Rect& rect{ std::get<Rect>(shape) };
	auto draw_origin{ GetDrawOrigin(entity) };
	return rect.Offset(transform, draw_origin);
}

V2_float ApplyTransform(
	const V2_float& local_point, const V2_float& position, const V2_float& scale, float cos_angle,
	float sin_angle
) {
	PTGN_ASSERT(!scale.IsZero(), "Cannot get world point for an object with zero scale");
	return position + (scale * local_point).Rotated(cos_angle, sin_angle);
}

V2_float ApplyTransform(
	const V2_float& local_point, const V2_float& position, const V2_float& scale
) {
	PTGN_ASSERT(!scale.IsZero(), "Cannot get world point for an object with zero scale");
	return position + scale * local_point;
}

V2_float ApplyTransform(const V2_float& local_point, const Transform& transform) {
	if (transform.GetRotation() == 0.0f) {
		return ApplyTransform(local_point, transform.GetPosition(), transform.GetScale());
	}
	return ApplyTransform(
		local_point, transform.GetPosition(), transform.GetScale(),
		std::cos(transform.GetRotation()), std::sin(transform.GetRotation())
	);
}

void ApplyTransform(
	const V2_float* local_points, std::size_t count, V2_float* out_world_points,
	const Transform& transform
) {
	if (transform.GetRotation() == 0.0f) {
		if (transform == Transform{}) {
			for (std::size_t i{ 0 }; i < count; ++i) {
				out_world_points[i] = local_points[i];
			}
		}
		for (std::size_t i{ 0 }; i < count; ++i) {
			out_world_points[i] =
				ApplyTransform(local_points[i], transform.GetPosition(), transform.GetScale());
		}
	} else {
		const float cosA = std::cos(transform.GetRotation());
		const float sinA = std::sin(transform.GetRotation());

		for (std::size_t i{ 0 }; i < count; ++i) {
			out_world_points[i] = ApplyTransform(
				local_points[i], transform.GetPosition(), transform.GetScale(), cosA, sinA
			);
		}
	}
}

std::vector<V2_float> ApplyTransform(
	const std::vector<V2_float>& local_points, const Transform& transform
) {
	std::vector<V2_float> world_points(local_points.size());
	ApplyTransform(local_points.data(), local_points.size(), world_points.data(), transform);
	return world_points;
}

namespace impl {

std::vector<V2_float> GetVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
) {
	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc_angle{ end_angle - start_angle };

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

float TriangulateArea(const V2_float* contour, std::size_t count) {
	PTGN_ASSERT(contour != nullptr);
	auto n = static_cast<int>(count);

	float A = 0.0f;

	for (int p = n - 1, q = 0; q < n; p = q++) {
		A += contour[p].x * contour[q].y - contour[q].x * contour[p].y;
	}
	return A * 0.5f;
}

// TODO: Combine x and ys.
bool TriangulateInsideTriangle(
	float Ax, float Ay, float Bx, float By, float Cx, float Cy, float Px, float Py
) {
	float ax  = Cx - Bx;
	float ay  = Cy - By;
	float bx  = Ax - Cx;
	float by  = Ay - Cy;
	float cx  = Bx - Ax;
	float cy  = By - Ay;
	float apx = Px - Ax;
	float apy = Py - Ay;
	float bpx = Px - Bx;
	float bpy = Py - By;
	float cpx = Px - Cx;
	float cpy = Py - Cy;

	float aCROSSbp = ax * bpy - ay * bpx;
	float cCROSSap = cx * apy - cy * apx;
	float bCROSScp = bx * cpy - by * cpx;

	return ((aCROSSbp >= 0.0f) && (bCROSScp >= 0.0f) && (cCROSSap >= 0.0f));
}

bool TriangulateSnip(
	const V2_float* contour, int u, int v, int w, int n, const std::vector<int>& V
) {
	PTGN_ASSERT(contour != nullptr);
	float Ax = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(u)])].x;
	float Ay = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(u)])].y;

	float Bx = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(v)])].x;
	float By = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(v)])].y;

	float Cx = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(w)])].x;
	float Cy = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(w)])].y;

	if (float res = (Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax); NearlyEqual(res, 0.0f)) {
		return false;
	}

	for (int p = 0; p < n; p++) {
		if ((p == u) || (p == v) || (p == w)) {
			continue;
		}
		float Px = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(p)])].x;
		float Py = contour[static_cast<std::size_t>(V[static_cast<std::size_t>(p)])].y;
		if (TriangulateInsideTriangle(Ax, Ay, Bx, By, Cx, Cy, Px, Py)) {
			return false;
		}
	}

	return true;
}

std::vector<std::array<V2_float, 3>> Triangulate(const V2_float* contour, std::size_t count) {
	// From: https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml
	PTGN_ASSERT(contour != nullptr);
	/* allocate and initialize list of Vertices in polygon */
	std::vector<std::array<V2_float, 3>> result;

	auto n = static_cast<int>(count);
	if (n < 3) {
		return result;
	}

	std::vector<int> V(static_cast<std::size_t>(n));

	/* we want a counter-clockwise polygon in V */

	if (0.0f < impl::TriangulateArea(contour, count)) {
		for (int v = 0; v < n; v++) {
			V[static_cast<std::size_t>(v)] = v;
		}
	} else {
		for (int v = 0; v < n; v++) {
			V[static_cast<std::size_t>(v)] = (n - 1) - v;
		}
	}

	int nv = n;

	/*  remove nv-2 Vertices, creating 1 triangle every time */
	int r_count = 2 * nv; /* error detection */

	for ([[maybe_unused]] int m = 0, v = nv - 1; nv > 2;) {
		/* if we loop, it is probably a non-simple polygon */
		if (0 >= (r_count--)) {
			//** Triangulate: ERROR - probable bad polygon!
			return result;
		}

		/* three consecutive vertices in current polygon, <u,v,w> */
		int u = v;
		if (nv <= u) {
			u = 0; /* previous */
		}
		v = u + 1;
		if (nv <= v) {
			v = 0; /* new v    */
		}
		int w = v + 1;
		if (nv <= w) {
			w = 0; /* next     */
		}

		if (TriangulateSnip(contour, u, v, w, nv, V)) {
			/* true names of the vertices */
			int a = V[static_cast<std::size_t>(u)];
			int b = V[static_cast<std::size_t>(v)];
			int c = V[static_cast<std::size_t>(w)];

			result.emplace_back(std::array<V2_float, 3>{ contour[static_cast<std::size_t>(a)],
														 contour[static_cast<std::size_t>(b)],
														 contour[static_cast<std::size_t>(c)] });

			m++;

			/* remove v from remaining polygon */
			for (int t = v + 1; t < nv; t++) {
				int s = t - 1;
				PTGN_ASSERT(s < static_cast<int>(V.size()));
				PTGN_ASSERT(t < static_cast<int>(V.size()));
				V[static_cast<std::size_t>(s)] = V[static_cast<std::size_t>(t)];
			}
			nv--;

			/* resest error detection counter */
			r_count = 2 * nv;
		}
	}

	return result;
}

/*
// TODO: Remove these once shaders for these shapes have been implemented.
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

/*
Rect RoundedRect::GetInnerRect() const {
	return { position - GetOffsetFromCenter(size, origin), size - V2_float{ radius } * 2.0f,
			 Origin::Center, rotation };
}

Rect RoundedRect::GetOuterRect() const {
	return { position - GetOffsetFromCenter(size, origin), size + V2_float{ radius } * 2.0f,
			 Origin::Center, rotation };
}*/

/*
void RoundedRect::Draw(
	const Color& color, float line_width, std::int32_t render_layer, const V2_float& rotation_center
) const {
	PTGN_ASSERT(
		2.0f * radius < size.x,
		"Cannot draw rounded rectangle with larger radius than half its width"
	);
	PTGN_ASSERT(
		2.0f * radius < size.y,
		"Cannot draw rounded rectangle with larger radius than half its height"
	);

	PTGN_ASSERT(
		line_width > 0.0f || line_width == -1.0f,
		"Cannot draw rounded rectangle with negative line width"
	);

	auto norm_color{ color.Normalized() };

	bool solid{ line_width == -1.0f };

	// If solid, this function draws the following shapes:
	// 4 radius thick lines, 4 solid arcs, 1 solid inner rectangle.
	//	  ------
	//  / ------ \
	// ||| **** |||
	// ||| **** |||
	//  \ ------ /
	//    ------
	// If hollow, this function draws the following shapes:
	// 4 thick lines, 4 thick arcs.
	//	 ------
	// /        \
	// |        |
	// |        |
	// \        /
	//   ------

	float angle1{ ClampAngle2Pi(rotation - pi<float>) };
	float angle2{ ClampAngle2Pi(rotation - half_pi<float>) };
	float angle3{ ClampAngle2Pi(rotation) };
	float angle4{ ClampAngle2Pi(rotation + half_pi<float>) };
	float angle5{
		ClampAngle2Pi(rotation + pi<float>)
	}; // same as angle1 but ensures correct direction.

	Rect inner_rect{ GetInnerRect() };
	auto vertices{ inner_rect.GetVertices(rotation_center) };

	Arc a1{ vertices[0], radius, angle1, angle2 };
	Arc a2{ vertices[1], radius, angle2, angle3 };
	Arc a3{ vertices[2], radius, angle3, angle4 };
	Arc a4{ vertices[3], radius, angle4, angle5 };

	if (solid) {
		a1.DrawSolid(false, a1.start_angle, a1.end_angle, norm_color, render_layer);
		a2.DrawSolid(false, a2.start_angle, a2.end_angle, norm_color, render_layer);
		a3.DrawSolid(false, a3.start_angle, a3.end_angle, norm_color, render_layer);
		a4.DrawSolid(false, a4.start_angle, a4.end_angle, norm_color, render_layer);
	} else {
		a1.DrawThick(line_width, false, a1.start_angle, a1.end_angle, norm_color, render_layer);
		a2.DrawThick(line_width, false, a2.start_angle, a2.end_angle, norm_color, render_layer);
		a3.DrawThick(line_width, false, a3.start_angle, a3.end_angle, norm_color, render_layer);
		a4.DrawThick(line_width, false, a4.start_angle, a4.end_angle, norm_color, render_layer);
	}

	float length{ radius };

	if (solid) {
		line_width	= radius;
		length	   /= 2.0f;
	}

	V2_float t{ V2_float(length, 0.0f).Rotated(rotation - half_pi<float>) };
	V2_float r{ V2_float(length, 0.0f).Rotated(rotation + 0.0f) };
	V2_float b{ V2_float(length, 0.0f).Rotated(rotation + half_pi<float>) };
	V2_float l{ V2_float(length, 0.0f).Rotated(rotation - pi<float>) };

	Line l1{ vertices[0] + t, vertices[1] + t };
	Line l2{ vertices[1] + r, vertices[2] + r };
	Line l3{ vertices[2] + b, vertices[3] + b };
	Line l4{ vertices[3] + l, vertices[0] + l };

	l1.DrawThick(line_width, norm_color, render_layer);
	l2.DrawThick(line_width, norm_color, render_layer);
	l3.DrawThick(line_width, norm_color, render_layer);
	l4.DrawThick(line_width, norm_color, render_layer);

	if (solid) {
		Rect::DrawSolid(norm_color, vertices, render_layer);
	}
}
*/
} // namespace impl

} // namespace ptgn