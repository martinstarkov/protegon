#pragma once

#include <array>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "math/geometry/line.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"

namespace ptgn {

// TODO: Move to tolerance.h

[[nodiscard]] bool StrictlyLess(
	float a, float b, float epsilon = std::numeric_limits<float>::epsilon()
);

[[nodiscard]] bool StrictlyLess(
	const V2_float& a, const V2_float& b, float epsilon = std::numeric_limits<float>::epsilon()
);

namespace impl {

// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
// (false).
// @param start_angle Must be in range: [0, 2pi).
// @param end_angle Must be in range: [0, 2pi).
// @return The vertices which make up the arc.
[[nodiscard]] std::vector<V2_float> GetArcVertices(
	const V2_float& center, float radius, float start_angle, float end_angle, bool clockwise
);

[[nodiscard]] float TriangulateArea(std::span<const V2_float> vertices);

// InsideTriangle decides if a point P is Inside of the triangle defined by A, B, C.
[[nodiscard]] bool TriangulateInsideTriangle(
	const V2_float& A, const V2_float& B, const V2_float& C, const V2_float& P
);

[[nodiscard]] bool TriangulateSnip(
	const V2_float* contour, std::size_t u, std::size_t v, std::size_t w, std::size_t n,
	const std::vector<std::size_t>& V
);

// @return A vector of triangles which make up the polygon contour.
[[nodiscard]] std::vector<std::array<V2_float, 3>> Triangulate(std::span<const V2_float> vertices);

enum class Orientation {
	LeftTurn  = 1,
	RightTurn = -1,
	Collinear = 0
};

/** Compute Orientation of 3 points in a plane.
 * @param a first point
 * @param b second point
 * @param c third point
 * @return Orientation of the points in the plane (left turn, right turn
 *         or Collinear)
 */
[[nodiscard]] Orientation GetOrientation(const V2_float& a, const V2_float& b, const V2_float& c);

bool VisibilityRayIntersects(
	const V2_float& origin, const V2_float& direction, const Line& segment, V2_float& out_point
);

struct VisibilityEvent {
	// events used in the visibility polygon algorithm
	enum Type {
		StartVertex,
		EndVertex
	};

	Type type;
	Line segment;
};

} // namespace impl

/* Calculate visibility polygon vertices in clockwise order.
 * Endpoints of the line segments (obstacles) can be ordered arbitrarily.
 * Line segments Collinear with the point are ignored.
 * @param point - position of the observer.
 * @param begin iterator of the list of line segments (obstacles).
 * @param end iterator of the list of line segments (obstacles).
 * @return vector of vertices of the visibility polygon.
 */
[[nodiscard]] std::vector<V2_float> GetVisibilityPolygon(
	const V2_float& origin, const std::vector<Line>& segments
);

[[nodiscard]] std::vector<Triangle> GetVisibilityTriangles(
	const V2_float& origin, const std::vector<Line>& segments
);

std::vector<Line> PointsToLines(const std::vector<V2_float>& points, bool connect_last_to_first);

namespace impl {

// @return True if point `p` is inside the edge (i.e., to the left of the edge from start to end).
[[nodiscard]] bool IsInside(const V2_float& p, const Line& edge);

// Computes intersection point between segment AB and line CD (clip edge).
// @return Nullopt if lines are parallel or no intersection on AB segment.
[[nodiscard]] std::optional<V2_float> ComputeIntersection(
	const V2_float& a, const V2_float& b, const V2_float& c, const V2_float& d
);

} // namespace impl

// Clips the subject polygon by the convex clip polygon using Sutherland-Hodgman algorithm.
// Both polygons are represented as vectors of points (in order).
[[nodiscard]] std::vector<V2_float> ClipPolygons(
	const std::vector<V2_float>& subject_polygon, const std::vector<V2_float>& clip_polygon
);

namespace impl {

[[nodiscard]] bool WithinPerimeter(float radius, float dist2, bool include_edge = false);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 149-150.
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. Function result is squared
// distance between between S1(s) and S2(t)
float ClosestPointLineLine(
	const V2_float& lineA_start, const V2_float& lineA_end, const V2_float& lineB_start,
	const V2_float& lineB_end, float& s, float& t, V2_float& c1, V2_float& c2
);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] float SquareDistancePointLine(
	const V2_float& point, const V2_float& start, const V2_float& end
);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] float SquareDistancePointRect(
	const V2_float& point, const V2_float& rect_min, const V2_float& rect_max
);

[[nodiscard]] float ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c);

[[nodiscard]] bool IntervalsOverlap(float min1, float max1, float min2, float max2);

// @return Amount by which the two intervals overlap. 0 is they do not overlap.
[[nodiscard]] float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
);

// @return True if all the interior angles are less than 180 degrees.
[[nodiscard]] bool IsConvexPolygon(const V2_float* vertices, std::size_t vertex_count);

// @return True if any of the interior angles are above 180 degrees.
[[nodiscard]] bool IsConcavePolygon(const V2_float* vertices, std::size_t vertex_count);

} // namespace impl

} // namespace ptgn