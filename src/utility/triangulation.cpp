#include "utility/triangulation.h"

#include <vector>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "utility/debug.h"

namespace ptgn::impl {

float TriangulateArea(const V2_float* contour, std::size_t count) {
	PTGN_ASSERT(contour != nullptr);
	auto n = static_cast<int>(count);

	float A = 0.0f;

	for (int p = n - 1, q = 0; q < n; p = q++) {
		A += contour[p].x * contour[q].y - contour[q].x * contour[p].y;
	}
	return A * 0.5f;
}

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

// From: https://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml
std::vector<Triangle> Triangulate(const V2_float* contour, std::size_t count) {
	PTGN_ASSERT(contour != nullptr);
	/* allocate and initialize list of Vertices in polygon */
	std::vector<Triangle> result;

	auto n = static_cast<int>(count);
	if (n < 3) {
		return result;
	}

	std::vector<int> V(static_cast<std::size_t>(n));

	/* we want a counter-clockwise polygon in V */

	if (0.0f < TriangulateArea(contour, count)) {
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

			result.emplace_back(contour[static_cast<std::size_t>(a)], contour[static_cast<std::size_t>(b)], contour[static_cast<std::size_t>(c)]);

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

} // namespace ptgn::impl
