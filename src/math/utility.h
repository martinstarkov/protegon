#pragma once

#include <utility>
#include <vector>

#include "math/geometry/axis.h"
#include "math/vector2.h"

namespace ptgn::impl {

[[nodiscard]] bool WithinPerimeter(float radius, float dist2);

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

[[nodiscard]] std::vector<Axis> GetPolygonAxes(
	const std::vector<V2_float>& vertices, bool intersection_info
);

// @return { min, max } of all the polygon vertices projected onto the given axis.
[[nodiscard]] std::pair<float, float> GetPolygonProjectionMinMax(
	const std::vector<V2_float>& vertices, const Axis& axis
);

[[nodiscard]] bool IntervalsOverlap(float min1, float max1, float min2, float max2);

// @return Amount by which the two intervals overlap. 0 is they do not overlap.
[[nodiscard]] float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
);

[[nodiscard]] bool IsConvexPolygon(const std::vector<V2_float>& vertices);

} // namespace ptgn::impl