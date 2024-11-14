#pragma once

#include <utility>
#include <vector>

#include "math/geometry/axis.h"
#include "math/vector2.h"

namespace ptgn {

struct Rect;
struct Polygon;
struct Line;

namespace impl {

[[nodiscard]] bool WithinPerimeter(float radius, float dist2);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 149-150.
// Computes closest points C1 and C2 of S1(s)=P1+s*(Q1-P1) and
// S2(t)=P2+t*(Q2-P2), returning s and t. Function result is squared
// distance between between S1(s) and S2(t)
float ClosestPointLineLine(
	const Line& line1, const Line& line2, float& s, float& t, V2_float& c1, V2_float& c2
);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] float SquareDistancePointLine(const Line& line, const V2_float& c);

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] float SquareDistancePointRect(const V2_float& a, const Rect& b);

[[nodiscard]] float ParallelogramArea(const V2_float& a, const V2_float& b, const V2_float& c);

[[nodiscard]] std::vector<Axis> GetAxes(const Polygon& polygon, bool intersection_info);

// @return { min, max } of all the polygon vertices projected onto the given axis.
[[nodiscard]] std::pair<float, float> GetProjectionMinMax(const Polygon& polygon, const Axis& axis);

[[nodiscard]] bool IntervalsOverlap(float min1, float max1, float min2, float max2);

// @return Amount by which the two intervals overlap. 0 is they do not overlap.
[[nodiscard]] float GetIntervalOverlap(
	float min1, float max1, float min2, float max2, bool contained_polygon,
	V2_float& out_axis_direction
);

} // namespace impl

} // namespace ptgn