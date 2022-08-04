#pragma once

#include "collision/fixed/FixedPointCircle.h"
#include "collision/fixed/FixedCircleCircle.h"
#include "collision/fixed/FixedAABBAABB.h"
#include "collision/fixed/FixedPointAABB.h"
#include "collision/fixed/FixedCapsuleCapsule.h"
#include "collision/fixed/FixedCircleCapsule.h"
#include "collision/fixed/FixedLineCapsule.h"
#include "collision/fixed/FixedLineLine.h"
#include "collision/fixed/FixedPointCapsule.h"

// point circle
// collision point = closest point on perimeter of circle to the point.
// normal = vector from collision point to point (unit).
// penetration depth = magnitude of non normalized normal.

// point aabb
// collision point = closest point on perimeter of aabb to the point.
// normal = vector from collision point to point (unit).
// penetration depth = magnitude of non normalized normal.

// line line
// collision point = point of intersection of both lines.
// normal = the direction vector toward the closest of the 4 end points (unit).
// penetration depth = magnitude of non normalized normal.
// special case: lines are collinear -> collision point = average of both line center points
//                                   -> penetration depth = 0 (special case)
//                                   -> tangent to either line's direction vector. 

// line circle
// collision point = point on the line that intersects the perimeter of the circle with longer distance to the end.
// normal = vector from closest point on line to circle origin to closest point on perimeter.
// penetration depth = magnitude of non normalized normal.

// line aabb
// collision point = point on the line that intersects an edge of the aabb with longer distance to the end of the line.
// normal = vector from collision point perpendicular to the collision edge of the aabb (unit).
// penetration depth = find which of the line end points is in the opposite of the direction of the normal, take the distance from that end point to the collision point in the direction of the normal
// https://noonat.github.io/intersect/#sweeping-an-aabb-through-multiple-objects:~:text=sy)%3B%0A%20%20%20%20%7D%0A%20%20%20%20return%20hit%3B%0A%20%20%7D-,AABB%20VS%20SWEPT%20AABB,-The%20sweep%20test

// circle circle
// collision point = shortest distance between circle centers and circle perimeters.
// normal = vector from one circle center to the collision point (unit).s
// penetration depth = magnitude of non normalized normal.


