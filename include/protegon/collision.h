#pragma once

#include "vector2.h"
#include "rectangle.h"
#include "circle.h"
#include "line.h"

namespace ptgn {

namespace impl {

float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b);

float ParallelogramArea(const Point<float>& a,
						const Point<float>& b,
						const Point<float>& c);

void ClosestPointSegment(const Point<float>& A,
						 const Segment<float>& B,
						 float& out_t,
						 Point<float>& out_d);

} // namespace impl

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
bool RectangleRectangle(const Rectangle<float>& a,
					    const Rectangle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.
bool CircleCircle(const Circle<float>& a,
				  const Circle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.
bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b);

bool PointRectangle(const Point<float>& a,
					const Rectangle<float>& b);

bool PointCircle(const Point<float>& a,
				 const Circle<float>& b);

// Source: https://www.jeffreythompson.org/collision-detection/line-point.php
// Source: https://stackoverflow.com/a/7050238
// Source (used): PointToLineSquareDistance == 0 but optimized slightly.
bool PointSegment(const Point<float>& a,
                  const Segment<float>& b);

bool SegmentRectangle(const Segment<float>& a,
				      const Rectangle<float>& b);

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection
bool SegmentCircle(const Segment<float>& a,
			       const Circle<float>& b);

// Source: https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
// With some modifications.
bool SegmentSegment(const Segment<float>& a,
			        const Segment<float>& b);

} // namespace overlap

namespace intersect {

struct Collision {
	float depth{ 0.0f };
	V2_float normal{ 0.0f, 0.0f };
};

bool RectangleRectangle(const Rectangle<float>& a,
						const Rectangle<float>& b,
						Collision& c);

bool CircleCircle(const Circle<float>& a,
				  const Circle<float>& b,
				  Collision& c);

bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b,
					 Collision& c);

} // namespace intersect

namespace dynamic {



} // namespace dynamic

} // namespace ptgn