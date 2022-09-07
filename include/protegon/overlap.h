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
bool PointLine(const Point<float>& a,
               const Line<float>& b);

bool LineRectangle(const Line<float>& a,
				   const Rectangle<float>& b);

// Source: https://www.jeffreythompson.org/collision-detection/line-circle.php
// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 179.
// Source: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
// Source (used): https://www.baeldung.com/cs/circle-line-segment-collision-detection
bool LineCircle(const Line<float>& a,
			    const Circle<float>& b);

// Source: https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
// With some modifications.
bool LineLine(const Line<float>& a,
			  const Line<float>& b);

} // namespace overlap

} // namespace ptgn