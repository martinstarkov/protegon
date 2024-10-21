#pragma once

#include <utility>
#include <vector>

#include "math/geometry/axis.h"
#include "math/vector2.h"

namespace ptgn {

struct Rect;
struct Polygon;

namespace impl {

// Source:
// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] float SquareDistancePointRectangle(const V2_float& a, const Rect& b);

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