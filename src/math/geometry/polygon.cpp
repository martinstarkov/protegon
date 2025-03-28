#include "math/geometry/polygon.h"

#include <array>
#include <cmath>
#include <vector>

#include "core/transform.h"
#include "math/math.h"
#include "math/utility.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/assert.h"

namespace ptgn {

namespace impl {

std::array<V2_float, 4> GetVertices(const Transform& transform, Rect rect) {
	rect.size *= transform.scale;

	auto half{ rect.Half() };

	V2_float top_left{ -half };
	V2_float top_right{ half.x, -half.y };
	V2_float bottom_right{ half };
	V2_float bottom_left{ -half.x, half.y };

	float cos{ 1.0f };
	float sin{ 0.0f };

	if (!NearlyEqual(transform.rotation, 0.0f)) {
		cos = std::cos(transform.rotation);
		sin = std::sin(transform.rotation);
	}

	auto center{ transform.position + rect.GetCenterOffset() };

	auto rotated = [&](const V2_float& point) {
		return center + V2_float{ cos * point.x - sin * point.y, sin * point.x + cos * point.y };
	};

	return { rotated(top_left), rotated(top_right), rotated(bottom_right), rotated(bottom_left) };
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
};

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

} // namespace impl

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

		if (impl::TriangulateSnip(contour, u, v, w, nv, V)) {
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

V2_float GetCenter(const Transform& transform, Rect rect) {
	rect.size *= transform.scale;
	return transform.position + rect.GetCenterOffset();
}

V2_float GetPolygonCenter(const V2_float* vertices, std::size_t vertex_count) {
	// Source: https://stackoverflow.com/a/63901131
	V2_float centroid;
	float signed_area{ 0.0f };
	V2_float v0{ 0.0f }; // Current verte
	V2_float v1{ 0.0f }; // Next vertex
	float a{ 0.0f };	 // Partial signed area

	std::size_t lastdex	 = vertex_count - 1;
	const V2_float* prev = &(vertices[lastdex]);
	const V2_float* next{ nullptr };

	// For all vertices in a loop
	for (std::size_t i{ 0 }; i < vertex_count; i++) {
		const auto& vertex{ vertices[i] };
		next		 = &vertex;
		v0			 = *prev;
		v1			 = *next;
		a			 = v0.Cross(v1);
		signed_area += a;
		centroid	+= (v0 + v1) * a;
		prev		 = next;
	}

	signed_area *= 0.5f;
	centroid	/= 6.0f * signed_area;

	return centroid;
}

Triangle::Triangle(const V2_float& a, const V2_float& b, const V2_float& c) : vertices{ a, b, c } {}

Triangle::Triangle(const std::array<V2_float, 3>& vertices) : vertices{ vertices } {}

Rect::Rect(const V2_float& size, Origin origin) : size{ size }, origin{ origin } {}

V2_float Rect::Half() const {
	return size * 0.5f;
}

V2_float Rect::GetCenterOffset() const {
	return GetOriginOffset(origin, size);
}

/*
RoundedRect::RoundedRect(
	const V2_float& position, float radius, const V2_float& size, Origin origin, float rotation
) :
	Rect{ position, size, origin, rotation } {
	PTGN_ASSERT(radius >= 0.0f);
	this->radius = radius;
}

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

Polygon::Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {
	PTGN_ASSERT(vertices.size() >= 3, "Cannot construct polygon from less than 3 vertices");
}

bool Polygon::IsConvex() const {
	return impl::IsConvexPolygon(vertices.data(), vertices.size());
}

bool Polygon::IsConcave() const {
	return !IsConvex();
}

} // namespace ptgn