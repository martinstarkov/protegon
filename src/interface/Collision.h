#pragma once

#include <algorithm>
#include <tuple>

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/Rectangle.h"
#include "core/ECS.h"
#include "physics/Collider.h"
#include "physics/Shape.h"
#include "core/Transform.h"
#include "physics/RigidBody.h"

// TODO: TEMPORARY
#include "renderer/Colors.h"
#include "debugging/Debug.h"
#include "interface/Input.h"
#include "interface/Draw.h"

namespace ptgn {

namespace collision {

struct CollisionManifold {
    CollisionManifold() = default;
    CollisionManifold(const V2_double& point, const V2_double& normal, const double time, bool occurs = true) : 
        point{ point }, normal{ normal }, time{ time }, occurs{ occurs } {}
    V2_double point;
    V2_double normal;
    double time{ 0 };
    double distance{ std::numeric_limits<double>::max() };
    bool occurs{ false };
};

// Returns an AABB which encompasses the initial position and the future position of a dynamic AABB.
inline std::pair<V2_double, V2_double> GetBroadphaseBox(const V2_double& velocity, const V2_double& position, const V2_double& size) {
    V2_double broadphase_position;
    V2_double broadphase_size;
    broadphase_position.x = velocity.x > 0.0 ? position.x : position.x + velocity.x;
    broadphase_position.y = velocity.y > 0.0 ? position.y : position.y + velocity.y;
    broadphase_size.x = velocity.x > 0.0 ? velocity.x + size.x : size.x - velocity.x;
    broadphase_size.y = velocity.y > 0.0 ? velocity.y + size.y : size.y - velocity.y;
    return { broadphase_position, broadphase_size };
}
// Determine if a point lies inside an AABB
inline bool PointVsAABB(const V2_double& point, const V2_double& position, const V2_double& size) {
    return (point.x >= position.x &&
            point.y >= position.y &&
            point.x < position.x + size.x &&
            point.y < position.y + size.y);
}
// Find the penetration of one AABB into another AABB
inline V2_double IntersectAABB(const V2_double& position1, const V2_double& size1, const V2_double& position2, const V2_double& size2) {
    V2_double penetration;
    double dx = position1.x - position2.x;
    auto center1 = size1 / 2.0;
    auto center2 = size2 / 2.0;
    double px = (center1.x + center2.x) - std::abs(dx);
    if (px <= 0.0) {
        return penetration;
    }
    double dy = position1.y - position2.y;
    double py = (center1.y + center2.y) - std::abs(dy);
    if (py <= 0.0) {
        return penetration;
    }
    if (px < py) {
        double sx = math::Sign(dx);
        penetration.x = px * sx;
    } else {
        double sy = math::Sign(dy);
        penetration.y = py * sy;
    }
    return penetration;
}
// Check if two AABBs overlap
inline bool AABBVsAABB(const V2_double& position1, const V2_double& size1, const V2_double& position2, const V2_double& size2) {
    // If any side of the aabb it outside the other aabb, there cannot be an overlap.
    if (position1.x + size1.x <= position2.x || position1.x >= position2.x + size2.x) return false;
    if (position1.y + size1.y <= position2.y || position1.y >= position2.y + size2.y) return false;
    return true;
}
// Check if a ray collides with an AABB.

inline void IntersectsVertex(int32_t i0, int32_t i1, V2_double const& K,
                             double q0, double q1, double q2, CollisionManifold& result) {
    // TODO: Possibly change later to account for the type of collision.
    result.occurs = true; // +1
    result.time = (q0 - std::sqrt(q1)) / q2;
    result.point[i0] = K[i0];
    result.point[i1] = K[i1];
}

inline void IntersectsEdge(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
                           double radius, V2_double const& V, CollisionManifold& result) {
    // TODO: Possibly change later to account for the type of collision.
    result.occurs = true; // +1
    result.time = (K0[i0] + radius - C[i0]) / V[i0];
    result.point[i0] = K0[i0];
    result.point[i1] = C[i1] + result.time * V[i1];
}


inline void InteriorOverlap(V2_double const& C, CollisionManifold& result) {
    // TODO: Possibly change later to account for the type of collision.
    result.occurs = true; // -1
    result.time = (double)0;
    result.point = C;
}

inline void EdgeOverlap(int32_t i0, int32_t i1, V2_double const& K, V2_double const& C,
                        V2_double const& delta, double radius, CollisionManifold& result) {
    // TODO: Possibly change later to account for the type of collision.
    result.occurs = delta[i0] < radius ? true : true; // -1, +1
    if (math::Compare(delta[i0], radius)) {
        // TODO: Figure out this corner clipping issue...
        result.time = (double)0.0;
    } else if (delta[i0] < radius) {
        // Check exact corner case
        result.time = (double)0.0;
    } else {
        result.time = 0.0;
    }
    result.point[i0] = K[i0];
    result.point[i1] = C[i1];
}

inline void VertexOverlap(V2_double const& K0, V2_double const& delta,
                          double radius, CollisionManifold& result) {
    double sqrDistance = delta[0] * delta[0] + delta[1] * delta[1];
    double sqrRadius = radius * radius;
    // TODO: Possibly change later to account for the type of collision.
    result.occurs = sqrDistance < sqrRadius ? true : true; // -1, +1
    if (math::Compare(sqrDistance, sqrRadius)) {
        // TODO: Figure out this corner clipping issue...
        result.time = (double)0.0;
    } else if (sqrDistance < sqrRadius) {
        // Check exact corner case
        result.time = (double)0.0;
    } else {
        result.time = 0.0;
    }
    result.point = K0;
}

inline void VertexSeparated(V2_double const& K0, V2_double const& delta0,
                            V2_double const& V, double radius, CollisionManifold& result) {
    double q0 = -V.DotProduct(delta0);
    if (q0 > (double)0) {
        double dotVPerpD0 = V.DotProduct(delta0.Tangent());
        double q2 = V.DotProduct(V);
        double q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
        if (q1 >= (double)0) {
            IntersectsVertex(0, 1, K0, q0, q1, q2, result);
        }
    }
}

inline void EdgeUnbounded(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
                          double radius, V2_double const& delta0, V2_double const& V, CollisionManifold& result) {
    if (V[i0] < (double)0) {
        double dotVPerpD0 = V[i0] * delta0[i1] - V[i1] * delta0[i0];
        if (radius * V[i1] + dotVPerpD0 >= (double)0) {
            V2_double K1, delta1;
            K1[i0] = K0[i0];
            K1[i1] = -K0[i1];
            delta1[i0] = C[i0] - K1[i0];
            delta1[i1] = C[i1] - K1[i1];
            double dotVPerpD1 = V[i0] * delta1[i1] - V[i1] * delta1[i0];
            if (radius * V[i1] + dotVPerpD1 <= (double)0) {
                IntersectsEdge(i0, i1, K0, C, radius, V, result);
            } else {
                double q2 = V.DotProduct(V);
                double q1 = radius * radius * q2 - dotVPerpD1 * dotVPerpD1;
                if (q1 >= (double)0) {
                    double q0 = -(V[i0] * delta1[i0] + V[i1] * delta1[i1]);
                    IntersectsVertex(i0, i1, K1, q0, q1, q2, result);
                }
            }
        } else {
            double q2 = V.DotProduct(V);
            double q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
            if (q1 >= (double)0) {
                double q0 = -(V[i0] * delta0[i0] + V[i1] * delta0[i1]);
                IntersectsVertex(i0, i1, K0, q0, q1, q2, result);
            }
        }
    }
}

inline void VertexUnbounded(V2_double const& K0, V2_double const& C, double radius,
                            V2_double const& delta0, V2_double const& V, CollisionManifold& result) {
    if (V[0] < (double)0 && V[1] < (double)0) {
        double dotVPerpD0 = V.DotProduct(delta0.Tangent());
        if (radius * V[0] - dotVPerpD0 <= (double)0) {
            if (-radius * V[1] - dotVPerpD0 >= (double)0) {
                double q2 = V.DotProduct(V);
                double q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
                double q0 = -V.DotProduct(delta0);
                IntersectsVertex(0, 1, K0, q0, q1, q2, result);
            } else {
                V2_double K1{ K0[0], -K0[1] };
                V2_double delta1 = C - K1;
                double dotVPerpD1 = V.DotProduct(delta1.Tangent());
                if (-radius * V[1] - dotVPerpD1 >= (double)0) {
                    IntersectsEdge(0, 1, K0, C, radius, V, result);
                } else {
                    double q2 = V.DotProduct(V);
                    double q1 = radius * radius * q2 - dotVPerpD1 * dotVPerpD1;
                    if (q1 >= (double)0) {
                        double q0 = -V.DotProduct(delta1);
                        IntersectsVertex(0, 1, K1, q0, q1, q2, result);
                    }
                }
            }
        } else {
            V2_double K2{ -K0[0], K0[1] };
            V2_double delta2 = C - K2;
            double dotVPerpD2 = V.DotProduct(delta2.Tangent());
            if (radius * V[0] - dotVPerpD2 <= (double)0) {
                IntersectsEdge(1, 0, K0, C, radius, V, result);
            } else {
                double q2 = V.DotProduct(V);
                double q1 = radius * radius * q2 - dotVPerpD2 * dotVPerpD2;
                if (q1 >= (double)0) {
                    double q0 = -V.DotProduct(delta2);
                    IntersectsVertex(1, 0, K2, q0, q1, q2, result);
                }
            }
        }
    }
}
inline void DoQuery(V2_double const& K, V2_double const& C,
                    double radius, V2_double const& V, CollisionManifold& result) {
    V2_double delta = C - K;
    if (delta[1] < radius) {
        if (delta[0] < radius) {
            if (delta[1] < (double)0) {
                if (delta[0] < (double)0) {
                    InteriorOverlap(C, result);
                } else {
                    EdgeOverlap(0, 1, K, C, delta, radius, result);
                }
            } else {
                if (delta[0] < (double)0) {
                    EdgeOverlap(1, 0, K, C, delta, radius, result);
                } else {
                    if (delta.DotProduct(delta) < radius * radius) {
                        VertexOverlap(K, delta, radius, result);
                    } else {
                        VertexSeparated(K, delta, V, radius, result);
                    }
                }

            }
        } else {
            EdgeUnbounded(0, 1, K, C, radius, delta, V, result);
        }
    } else {
        if (delta[0] < radius) {
            EdgeUnbounded(1, 0, K, C, radius, delta, V, result);
        } else {
            VertexUnbounded(K, C, radius, delta, V, result);
        }
    }
}

inline CollisionManifold CircleVsAABB(double dt, const V2_double& box_position, const V2_double& box_size, V2_double const& boxVelocity,
                                      const V2_double& circle_center, double circle_radius, V2_double const& circleVelocity) {
    CollisionManifold result{};
    result.time = 1.0;
    // Translate the circle and box so that the box center becomes
    // the origin.  Compute the velocity of the circle relative to
    // the box.

    auto box_min = box_position;
    auto box_max = box_position + box_size;

    V2_double extent = box_size / 2;
    V2_double boxCenter = box_position + extent;
    V2_double C = circle_center - boxCenter;
    V2_double V = (circleVelocity - boxVelocity);

    // Change signs on components, if necessary, to transform C to the
    // first quadrant.  Adjust the velocity accordingly.
    std::array<double, 2> sign = { (double)0, (double)0 };
    for (int32_t i = 0; i < 2; ++i) {
        if (C[i] >= (double)0) {
            sign[i] = (double)1;
        } else {
            C[i] = -C[i];
            V[i] = -V[i];
            sign[i] = (double)-1;
        }
    }

    DoQuery(extent, C, circle_radius, V, result);

    if (result.occurs) {

        // Translate back to the original coordinate system.
        for (int32_t i = 0; i < 2; ++i) {
            if (sign[i] < (double)0) {
                result.point[i] = -result.point[i];
            }
        }

        result.point += boxCenter;

        //auto new_box_center = boxCenter + V * result.time;
        result.normal = (circle_center + circleVelocity * result.time - result.point).Unit();
    }
    return result;
}

using SCALAR = double;
using VECTOR = V2_double;

// taken from https://github.com/pgkelley4/line-segments-intersect/blob/master/js/line-segments-intersect.js
// returns the point where they intersect (if they intersect)
// returns null if they don't intersect
double getRayIntersectionFractionOfFirstRay(V2_double originA, V2_double endA, V2_double originB, V2_double endB)
{
    auto r = endA - originA;
    auto s = endB - originB;

    double numerator = (originB - originA).DotProduct(r);
    double denominator = r.DotProduct(s);

    if (numerator == 0 && denominator == 0) {
        // the lines are co-linear
        // check if they overlap
        /*return	((originB.x - originA.x < 0) != (originB.x - endA.x < 0) != (endB.x - originA.x < 0) != (endB.x - endA.x < 0)) ||
                ((originB.y - originA.y < 0) != (originB.y - endA.y < 0) != (endB.y - originA.y < 0) != (endB.y - endA.y < 0));*/
        return V2_double::Maximum().x;
    }
    if (denominator == 0) {
        // lines are parallel
        return V2_double::Maximum().x;
    }

    double u = numerator / denominator;
    double t = ((originB - originA).DotProduct(s)) / denominator;
    if ((t >= 0) && (t <= 1) && (u >= 0) && (u <= 1)) {
        //return originA + (r * t);
        return t;
    }
    return V2_double::Maximum().x;
}

double getRayIntersectionFraction(V2_double origin, V2_double direction, V2_double min, V2_double max) {
    V2_double end = origin + direction;

    double minT = getRayIntersectionFractionOfFirstRay(origin, end, V2_double(min.x, min.y), V2_double(min.x, max.y));
    double x = getRayIntersectionFractionOfFirstRay(origin, end, V2_double(min.x, max.y), V2_double(max.x, max.y));
    if (x < minT)
        minT = x;
    x = getRayIntersectionFractionOfFirstRay(origin, end, V2_double(max.x, max.y), V2_double(max.x, min.y));
    if (x < minT)
        minT = x;
    x = getRayIntersectionFractionOfFirstRay(origin, end, V2_double(max.x, min.y), V2_double(min.x, min.y));
    if (x < minT)
        minT = x;

    // ok, now we should have found the fractional component along the ray where we collided
    return minT;
}

double lineToPlane(V2_double p, V2_double u, V2_double v, V2_double n) {
    auto NdotU = n.x * u.x + n.y * u.y;
    if (NdotU == 0) return std::numeric_limits<double>::infinity();

    // return n.(v-p) / n.u
    return (n.x * (v.x - p.x) + n.y * (v.y - p.y)) / NdotU;
}

bool between(double x, double a, double b) {
    return x > a && x < b;
}

double sweepAABB(V2_double a, V2_double ah, V2_double b, V2_double bh, V2_double d, V2_double& normal) {
    V2_double m;
    V2_double mh;

    m.x = b.x - (a.x + ah.x);
    m.y = b.y - (a.y + ah.y);
    mh.x = ah.x + bh.x;
    mh.y = ah.y + bh.y;

    double h = 1;

    normal = {};
    double xmin, xmax, ymin, ymax;
    // X min
    xmin = lineToPlane({ 0, 0 }, d, m, { -1, 0 });
    if (xmin >= 0 && d.x > 0 && between(xmin * d.y, m.y, m.y + mh.y)) { 
        if (xmin < h) {
            h = xmin;
            normal.x = -1;
            normal.y = 0;
        }
    }

    // X max
    xmax = lineToPlane({ 0, 0 }, d, { m.x + mh.x, m.y }, { 1, 0 });
    if (xmax >= 0 && d.x < 0 && between(xmax * d.y, m.y, m.y + mh.y)) {
        if (xmax < h) {
            h = xmax;
            normal.x = 1;
            normal.y = 0;
        }
    }

    // Y min
    ymin = lineToPlane({ 0, 0 }, d, m, { 0, -1 });
    if (ymin >= 0 && d.y > 0 && between(ymin * d.x, m.x, m.x + mh.x)) {
        if (ymin < h) {
            h = ymin;
            normal.y = -1;
            normal.x = 0;
        }
    }

    // Y max
    ymax = lineToPlane({ 0, 0 }, d, { m.x, m.y + mh.y }, { 0, 1 });
    if (ymax >= 0 && d.y < 0 && between(ymax * d.x, m.x, m.x + mh.x)) {
        if (ymax < h) {
            h = ymax;
            normal.y = 1;
            normal.x = 0;
        }
    }


    return h;
}


double rayaabb(V2_double v, V2_double a_min, V2_double a_max, V2_double b_min, V2_double b_max, double& tfirst, double& tlast) {
    // Initialize times of first and last contact
    tfirst = 0.0;
    tlast = 1.0;
    // For each axis, determine times of first and last contact, if any
    for (int i = 0; i < 2; i++) {
        if (v[i] < 0.0) {
            if (b_max[i] < a_min[i]) return 0; // Nonintersecting and moving apart
            if (a_max[i] < b_min[i]) tfirst = std::max((a_max[i] - b_min[i]) / v[i], tfirst);
            if (b_max[i] > a_min[i]) tlast = std::min((a_min[i] - b_max[i]) / v[i], tlast);
        }
        if (v[i] > 0.0) {
            if (b_min[i] > a_max[i]) return 0; // Nonintersecting and moving apart
            if (b_max[i] < a_min[i]) tfirst = std::max((a_min[i] - b_max[i]) / v[i], tfirst);
            if (a_max[i] > b_min[i]) tlast = std::min((a_max[i] - b_min[i]) / v[i], tlast);
        }
        // No overlap possible if time of first contact occurs after time of last contact
        if (tfirst > tlast) return 0;
    }
    return 1;
}

inline bool RayVsRectangle(const V2_double& ray_origin, const V2_double& ray_dir, 
                      const V2_double& position, const V2_double& size, 
                      CollisionManifold& out_collision) {

    // Initial condition: no collision normal.
    out_collision.normal = { 0.0, 0.0 };
    out_collision.point = { 0.0, 0.0 };

    // Cache division.
    auto inv_dir = 1.0 / ray_dir;

    // Calculate intersections with rectangle bounding axes.
    auto t_near = (position - ray_origin) * inv_dir;
    auto t_far = (position + size - ray_origin) * inv_dir;

    // Discard 0 / 0 divisions.
    if (std::isnan(t_far.y) || std::isnan(t_far.x)) return false;
    if (std::isnan(t_near.y) || std::isnan(t_near.x)) return false;

    // Sort axis collision times so t_near contains the shorter time.
    if (t_near.x > t_far.x) std::swap(t_near.x, t_far.x);
    if (t_near.y > t_far.y) std::swap(t_near.y, t_far.y);

    // Early rejection.
    if (t_near.x > t_far.y || t_near.y > t_far.x) return false;

    // Closest time will be the first contact.
    out_collision.time = std::max(t_near.x, t_near.y);

    // Furthest time is contact on opposite side of target.
    double t_hit_far = std::min(t_far.x, t_far.y);

    // Reject if furthest time is negative, meaning the object is travelling away from the target.
    if (t_hit_far < 0.0) {
        return false;
    }

    // Contact point of collision from parametric line equation.
    out_collision.point = ray_origin + out_collision.time * ray_dir;

    // Find which axis collides further along the movement time.

    // TODO: Figure out how to fix biasing of one direction from one side and another on the other side.
    if (math::Compare(t_near.x, t_near.y) && math::Compare(math::Abs(inv_dir.x), math::Abs(inv_dir.y))) { // Both axes collide at the same time.
        // Diagonal collision, set normal to opposite of direction of movement.
        out_collision.normal = ray_dir.Identity().Opposite();
    } if (t_near.x > t_near.y) { // X-axis.
        // Direction of movement.
        if (inv_dir.x < 0.0) {
            out_collision.normal = { 1.0, 0.0 };
        } else {
            out_collision.normal = { -1.0, 0.0 };
        }
    } else if (t_near.x < t_near.y) { // Y-axis.
        // Direction of movement.
        if (inv_dir.y < 0.0) {
            out_collision.normal = { 0.0, 1.0 };
        } else {
            out_collision.normal = { 0.0, -1.0 };
        }
    }

    // Raycast collision occurred.
    return true;
}


namespace internal {

template <typename T>
inline void SWAP(T& a, T& b)
//swap the values of a and b

{

    const T temp = a;
    a = b;
    b = temp;
}

// Return true if r1 and r2 are real
inline bool QuadraticFormula
(
    const SCALAR a,
    const SCALAR b,
    const SCALAR c,
    SCALAR& r1, //first
    SCALAR& r2 //and second roots
)

{


    const SCALAR q = b * b - 4 * a * c;
    if (q >= 0)

    {


        const SCALAR sq = sqrt(q);
        const SCALAR d = 1 / (2 * a);
        r1 = (-b + sq) * d;
        r2 = (-b - sq) * d;
        return true;//real roots

    }

    else

    {


        return false;//complex roots

    }

}

CollisionManifold SphereSphereSweep
(
    const SCALAR ra, //radius of sphere A
    const VECTOR& A0, //previous position of sphere A
    const VECTOR& A1, //current position of sphere A
    const SCALAR rb, //radius of sphere B
    const VECTOR& B0, //previous position of sphere B
    const VECTOR& B1, //current position of sphere B
    SCALAR& u0, //normalized time of first collision
    SCALAR& u1, //normalized time of second collision
    const V2_double& velocity,
    bool print_info = false
)

{

    const VECTOR va = A1 - A0;
    //vector from A0 to A1

    const VECTOR vb = B1 - B0;
    //vector from B0 to B1

    const VECTOR AB = B0 - A0;
    //vector from A0 to B0

    const VECTOR vab = vb - va;
    //relative velocity (in normalized time)

    const SCALAR rab = ra + rb;

    const SCALAR a = vab.DotProduct(vab);
    //u*u coefficient

    const SCALAR b = 2 * vab.DotProduct(AB);
    //u coefficient

    const SCALAR c = AB.DotProduct(AB) - rab * rab;
    //constant term
    //check if they're currently overlapping

    // TODO: Figure out how to reintroduce this without causing sticking.
    /*
    if (AB.DotProduct(AB) <= rab * rab)

    {

        if (print_info) debug::PrintLine("early exit");
        u0 = 0;
        u1 = 0;

        auto new_origin = A0 + velocity * u0;
        auto normal = (new_origin - B0).Unit();
        collision::CollisionManifold collision;
        collision.occurs = true;
        collision.distance = (A0 - B0).MagnitudeSquared();
        collision.normal = normal;
        collision.time = u0;
        collision.point = new_origin;


        return collision;

    }*/

    //check if they hit each other
    // during the frame
    if (QuadraticFormula(a, b, c, u0, u1)) {

        if (print_info) debug::PrintLine("later exit");

        if (u0 > u1)
            SWAP(u0, u1);
        // TODO: Check that this is accurate to theory:
        // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
        if (math::Compare(u0, u1)) {
            u0 = 1.0;
            u1 = 1.0;
        }
        // TODO: Clean this up.
        auto new_origin = A0 + velocity * u0;
        auto normal = (new_origin - B0).Unit();
        collision::CollisionManifold collision;
        collision.occurs = true;
        collision.distance = (A0 - B0).MagnitudeSquared();
        collision.normal = normal;
        collision.time = u0;
        collision.point = new_origin;


        return collision;

    }
    // TODO: Figure out if this is required.
    if (math::Compare(u0, u1)) {
        u0 = 1.0;
        u1 = 1.0;
    }
    collision::CollisionManifold collision;
    collision.time = 1.0;
    return collision;

}

void SortCollisionTimes(std::vector<CollisionManifold>& collisions) {
    // Initial sort based on distances of collision manifolds to the collider.
    // TODO: Figure out if the distance sort is required. I'm not sure but I think it
    // may be required in order to prioritize walls to corners, but then again the 
    // normal magnitude comparison may do this already so it may not be required.
    std::sort(collisions.begin(), collisions.end(), [](const CollisionManifold& a, const CollisionManifold& b) {
        return a.distance < b.distance;
    });
    std::sort(collisions.begin(), collisions.end(), [](const CollisionManifold& a, const CollisionManifold& b) {
        // If collision times are not equal, sort by collision time.
        if (!math::Compare(a.time, b.time)) {
            return a.time < b.time;
        } else {
            // If time of collision are equal, prioritize walls to corners, i.e. normals (1,0) come before (1,1).
            return a.normal.MagnitudeSquared() < b.normal.MagnitudeSquared();
        }
    });
}

V2_double GetNewVelocity(const V2_double& velocity, const CollisionManifold& collision) {
    V2_double new_velocity;
    double remaining_time = (1.0 - collision.time);
    double dot_product = velocity.DotProduct(collision.normal.Tangent());

    // SLIDE
    new_velocity = dot_product * collision.normal.Tangent() * remaining_time;

    // PUSH
    //if (dot_product > 0.0) { dot_product = 1.0; } else if (dot_product < 0.0) { dot_product = -1.0; }
    //new_velocity = dot_product * collision.normal.Flip() * remaining_time * velocity.Magnitude();

    // BOUNCE
    //new_velocity = velocity * remaining_time;
    //if (!math::Compare(math::Abs(collision.normal.x), 0.0)) new_velocity.x = -new_velocity.x;
    //if (!math::Compare(math::Abs(collision.normal.y), 0.0)) new_velocity.y = -new_velocity.y;

    return new_velocity;
}

// @return Struct containing collision information about the sweep.
CollisionManifold SweepRectangleVsRectangle(const V2_double& origin, const V2_double& size, const V2_double& velocity, const V2_double& target_position, const V2_double& target_size) {
    collision::CollisionManifold c;
    V2_double expanded_position{ target_position - target_size / 2.0 };
    V2_double expanded_size{ target_size + size };
    bool occured = collision::RayVsRectangle(origin, velocity, expanded_position, expanded_size, c);
    c.occurs = occured && c.time < 1.0 && (c.time > 0.0 || math::Compare(c.time, 0.0)) && !c.normal.IsZero();
    if (c.occurs) c.distance = (origin - (target_position + target_size / 2)).MagnitudeSquared();
    return c;
}

// @return Struct containing collision information about the sweep.
static CollisionManifold SweepCircleVsCircle(const V2_double& origin, const double& radius, const V2_double& velocity, const V2_double& target_position, const double& target_radius) {
    double close, far;
    collision::CollisionManifold c = collision::internal::SphereSphereSweep(radius, origin, origin + velocity, target_radius, target_position + target_radius, target_position + target_radius, close, far, velocity, false);
    c.occurs = c.occurs && c.time < 1.0 && (c.time > 0.0 || math::Compare(c.time, 0.0)) && !c.normal.IsZero();
    //if (print_info) debug::PrintLine("Occured: ", c.occurs, " at ", c.time);
    return c;
}

std::pair<CollisionManifold, V2_double> CircleAABBSweep(const V2_double& rectangle_position, const V2_double& rectangle_size, const V2_double& start, const V2_double& end, double radius) {
    const auto L = rectangle_position.x;
    const auto T = rectangle_position.y;
    const auto R = rectangle_position.x + rectangle_size.x;
    const auto B = rectangle_position.y + rectangle_size.y;
    // If the bounding box around the start and end points (+radius on all
    // sides) does not intersect with the rectangle, definitely not an
    // intersection
    if ((std::max(start.x, end.x) + radius < L) ||
        (std::min(start.x, end.x) - radius > R) ||
        (std::max(start.y, end.y) + radius < T) ||
        (std::min(start.y, end.y) - radius > B)) {
        return {};
    }
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto invdx = (math::Compare(dx, 0.0) ? 0.0 : 1.0 / dx);
    const auto invdy = (math::Compare(dy, 0.0) ? 0.0 : 1.0 / dy);
    V2_double corner = V2_double::Maximum();
    // Calculate intersection times with each side's plane
    // Check each side's quadrant for single-side intersection
    // Calculate Corner
    // Left Side
    // Does the circle go from the left side to the right side of the rectangle's left?
    if (start.x - radius < L && end.x + radius > L) {
        auto ltime = ((L - radius) - start.x) * invdx;
        if ((ltime > 0.0 || math::Compare(ltime, 0.0)) && (ltime < 1.0 || math::Compare(ltime, 1.0))) {
            auto ly = dy * ltime + start.y;
            // Does the collisions point lie on the left side?
            if ((ly > T || math::Compare(ly, T)) && (ly < B || math::Compare(ly, B))) {
                // circle center: { dx * ltime + start.x, ly }
                return { CollisionManifold({ L, ly }, { -1, 0 }, ltime), { dx * ltime + start.x, ly } };
            }
        }
        corner.x = L;
    }
    // Right Side
    // Does the circle go from the right side to the left side of the rectangle's right?
    if (start.x + radius > R && end.x - radius < R) {
        auto rtime = (start.x - (R + radius)) * -invdx;
        if ((rtime > 0.0 || math::Compare(rtime, 0.0)) && (rtime < 1.0 || math::Compare(rtime, 1.0))) {
            auto ry = dy * rtime + start.y;
            // Does the collisions point lie on the right side?
            if ((ry > T || math::Compare(ry, T)) && (ry < B || math::Compare(ry, B))) {
                // circle center: { dx * rtime + start.x, ry }
                return { CollisionManifold({ R, ry }, { 1, 0 }, rtime), { dx * rtime + start.x, ry } };
            }
        }
        corner.x = R;
    }
    // Top Side
    // Does the circle go from the top side to the bottom side of the rectangle's top?
    if (start.y - radius < T && end.y + radius > T) {
        auto ttime = ((T - radius) - start.y) * invdy;
        if ((ttime > 0.0 || math::Compare(ttime, 0.0)) && (ttime < 1.0 || math::Compare(ttime, 1.0))) {
            auto tx = dx * ttime + start.x;
            // Does the collisions point lie on the top side?
            if ((tx > L || math::Compare(tx, L)) && (tx < R || math::Compare(tx, R))) {
                // circle center: { tx, dy * ttime + start.y }
                return { CollisionManifold({ tx, T }, { 0, -1 }, ttime), { tx, dy * ttime + start.y } };
            }
        }
        corner.y = T;
    }
    // Bottom Side
    // Does the circle go from the bottom side to the top side of the rectangle's bottom?
    if (start.y + radius > B && end.y - radius < B) {
        auto btime = (start.y - (B + radius)) * -invdy;
        if ((btime > 0.0 || math::Compare(btime, 0.0)) && (btime < 1.0 || math::Compare(btime, 1.0))) {
            auto bx = dx * btime + start.x;
            // Does the collisions point lie on the bottom side?
            if ((bx > L || math::Compare(bx, L)) && (bx < R || math::Compare(bx, R))) {
                // circle center: { bx, dy * btime + start.y }
                return { CollisionManifold({ bx, B }, { 0, 1 }, btime), { bx, dy * btime + start.y } };
            }
        }
        corner.y = B;
    }
    // No intersection at all!
    if (math::Compare(corner.x, V2_double::Maximum().x) && math::Compare(corner.y, V2_double::Maximum().y)) {
        return {};
    }
    // Account for the times where we don't pass over a side but we do hit it's corner
    if (!math::Compare(corner.x, V2_double::Maximum().x) && math::Compare(corner.y, V2_double::Maximum().y)) {
        corner.y = (dy > 0.0 ? B : T);
    }
    if (!math::Compare(corner.y, V2_double::Maximum().y) && math::Compare(corner.x, V2_double::Maximum().x)) {
        corner.x = (dx > 0.0 ? R : L);
    }
    // Solve the triangle between the start, corner, and intersection point.
     //
     //           +-----------T-----------+
     //           |                       |
     //          L|                       |R
     //           |                       |
     //           C-----------B-----------+
     //          / \
     //         /   \r     _.-E
     //        /     \ _.-'
     //       /    _.-I
     //      / _.-'
     //     S-'
     //
     // S = start of circle's path
     // E = end of circle's path
     // LTRB = sides of the rectangle
     // I = {ix, iY} = point at which the circle intersects with the rectangle
     // C = corner of intersection (and collision point)
     // C=>I (r) = {nx, ny} = radius and intersection normal
     // S=>C = cornerdist
     // S=>I = intersectionDistance
     // S=>E = lineLength
     // <S = innerAngle
     // <I = angle1
     // <C = angle2
     //
    double inverseRadius = 1.0 / radius;
    double lineLength = std::sqrt(dx * dx + dy * dy);
    double cornerdx = corner.x - start.x;
    double cornerdy = corner.y - start.y;
    double cornerDistance = std::sqrt(cornerdx * cornerdx + cornerdy * cornerdy);
    double innerAngle = std::acos((cornerdx * dx + cornerdy * dy) / (lineLength * cornerDistance));
    if (std::isnan(innerAngle)) innerAngle = 0.0; //debug::PrintLine((cornerdx * dx + cornerdy * dy) / lineLength, ", ", (cornerDistance));
    
    // If the circle is too close, no intersection.
    if (cornerDistance < radius) {
        return {};
    }
    // If inner angle is zero, it's going to hit the corner straight on.
    if (math::Compare(innerAngle, 0.0)) {
        double time = (double)((cornerDistance - radius) / lineLength);
        // If time is outside the boundaries, return null. This algorithm can
        // return a negative time which indicates a previous intersection, and
        // can also return a time > 1.0 which can predict a corner intersection.
        if (time > 1.0 || time < 0.0) {
            return {};
        }
        auto ix = time * dx + start.x;
        auto iy = time * dy + start.y;
        auto nx = (double)(cornerdx / cornerDistance);
        auto ny = (double)(cornerdy / cornerDistance);
        // circle center: { ix, iy }
        return { CollisionManifold({ corner.x, corner.y }, { nx, ny }, time), { ix, iy } };
    }
    double innerAngleSin = std::sin(innerAngle);
    double angle1Sin = innerAngleSin * cornerDistance * inverseRadius;
    // The angle is too large, there cannot be an intersection
    if (std::abs(angle1Sin) > 1.0) {
        return {};
    }
    double angle1 = math::PI<double> - std::asin(angle1Sin);
    double angle2 = math::PI<double> - innerAngle - angle1;
    double intersectionDistance = radius * std::sin(angle2) / innerAngleSin;
    debug::PrintLine(intersectionDistance);
    // Solve for time
    double time = (double)(intersectionDistance / lineLength);
    if (math::Compare(time, 0.0)) time = 0.0;
    // If time is outside the boundaries, return null. This algorithm can
    // return a negative time which indicates a previous intersection, and
    // can also return a time > 1.0f which can predict a corner intersection.
    if (time > 1.0 || time < 0.0) {
        return {};
    }
    // Solve the intersection and normal
    double ix = time * dx + start.x;
    double iy = time * dy + start.y;
    double nx = (double)((ix - corner.x) * inverseRadius);
    double ny = (double)((iy - corner.y) * inverseRadius);
    // circle center: { ix, iy }
    return { CollisionManifold({ corner.x, corner.y }, { nx, ny }, time), { ix, iy } };
}


// Support function that returns the AABB vertex with index n
V2_double Corner(V2_double min, V2_double max, int n) {
    V2_double p;
    p.x = ((n & 1) ? max.x : min.x);
    p.y = ((n & 1) ? max.y : min.y);
    return p;
}
struct Segment{
    V2_double start;
    V2_double end;
};

// Intersect ray R(t) = p + t*d against AABB a. When intersecting,
// return intersection distance tmin and point q of intersection
int IntersectRayAABB(V2_double p, V2_double d, V2_double amax, V2_double amin, double& tmin, V2_double& q) {
    tmin = 0.0; // set to -FLT_MAX to get first hit on line
    double tmax = DBL_MAX; // set to max distance ray can travel (for segment)
    // For all three slabs
    for (int i = 0; i < 2; i++) {
        if (std::abs(d[i]) < math::EPSILON<double>) {
            // Ray is parallel to slab. No hit if origin not within slab
            if (p[i] < amin[i] || p[i] > amax[i]) return 0;
        } else {
            // Compute intersection t value of ray with near and far plane of slab
            double ood = 1.0 / d[i];
            double t1 = (amin[i] - p[i]) * ood;
            double t2 = (amax[i] - p[i]) * ood;
            // Make t1 be intersection with near plane, t2 with far plane
            if (t1 > t2) std::swap(t1, t2);
            // Compute the intersection of slab intersection intervals
            if (t1 > tmin) tmin = t1;
            if (t2 > tmax) tmax = t2;
            // Exit with no collision as soon as slab intersection becomes empty
            if (tmin > tmax) return 0;
        }
    }
    // Ray intersects all 3 slabs. Return point (q) and intersection t value (tmin)
    q = p + d * tmin;
    return 1;
}

// Intersect segment S(t)=sa+t(sb-sa), 0<=t<=1 against cylinder specified by p, q and r
int IntersectSegmentCapsule(V2_double sa, V2_double sb, V2_double p, V2_double q, double r, double& t) {
    V2_double d = q - p, m = sa - p, n = sb - sa;
    double md = m.DotProduct(d);
    double nd = n.DotProduct(d);
    double dd = d.DotProduct(d);
    // Test if segment fully outside either endcap of cylinder
    if (md < 0.0 && md + nd < 0.0) return 0; // Segment outside ’p’ side of cylinder
    if (md > dd && md + nd > dd) return 0; // Segment outside ’q’ side of cylinder
    double nn = n.DotProduct(n);
    double mn = m.DotProduct(n);
    double a = dd * nn - nd * nd;
    double k = m.DotProduct(m) - r * r;
    double c = dd * k - md * md;
    if (std::abs(a) < math::EPSILON<double>) {
        // Segment runs parallel to cylinder axis
        if (c > 0.0) return 0; // ’a’ and thus the segment lie outside cylinder
        // Now known that segment intersects cylinder; figure out how it intersects
        if (md < 0.0) t = -mn / nn; // Intersect segment against ’p’ endcap
        else if (md > dd) t = (nd - mn) / nn; // Intersect segment against ’q’ endcap
        else t = 0.0; // ’a’ lies inside cylinder
        return 1;
    }
    double b = dd * mn - nd * md;
    double discr = b * b - a * c;
    if (discr < 0.0) return 0; // No real roots; no intersection
    t = (-b - math::Sqrt(discr)) / a;
    if (t < 0.0 || t > 1.0) return 0; // Intersection lies outside segment
    if (md + t * nd < 0.0) {
        // Intersection outside cylinder on ’p’ side
        if (nd <= 0.0) return 0; // Segment pointing away from endcap
        t = -md / nd;
        // Keep intersection if Dot(S(t) - p, S(t) - p) <= r∧2
        return k + 2 * t * (mn + t * nn) <= 0.0;
    } else if (md + t * nd > dd) {
        // Intersection outside cylinder on ’q’ side
        if (nd >= 0.0) return 0; // Segment pointing away from endcap
        t = (dd - md) / nd;
        // Keep intersection if Dot(S(t) - q, S(t) - q) <= r∧2
        return k + dd - 2 * md + t * (2 * (mn - nd) + t * nn) <= 0.0;
    }
    // Segment intersects cylinder between the endcaps; t is correct
    return 1;
}
int IntersectMovingSphereAABB(V2_double sc, double sr, V2_double d, V2_double rect_pos, V2_double rect_size, double& t, V2_double& normal) {
    // Compute the AABB resulting from expanding b by sphere radius r
    auto min = rect_pos;
    auto max = rect_pos + rect_size;
    auto emin = rect_pos;
    auto emax = rect_pos + rect_size;
    emin.x -= sr; emin.y -= sr; 
    emax.x += sr; emax.y += sr;
    // Intersect ray against expanded AABB e. Exit with no intersection if ray
    // misses e, else get intersection point p and time t as result
    V2_double p;
    if (!IntersectRayAABB(sc, d, emin, emax, t, p) || t > 1.0)
        return 0;
    // Compute which min and max faces of b the intersection point p lies
    // outside of. Note, u and v cannot have the same bits set and
    // they must have at least one bit set among them
    int u = 0, v = 0;
    if (p.x < min.x) u |= 1;
    if (p.x > max.x) v |= 1;
    if (p.y < min.y) u |= 2;
    if (p.y > max.y) v |= 2;
    // ‘Or’ all set bits together into a bit mask (note: here u + v == u | v)
    int m = u + v;
    // Define line segment [c, c+d] specified by the sphere movement
    Segment seg{ sc, sc + d };
    // If all 3 bits set (m == 7) then p is in a vertex region
    if (m == 7) {
        // Must now intersect segment [c, c+d] against the capsules of the three
        // edges meeting at the vertex and return the best time, if one or more hit
        double tmin = DBL_MAX;
        if (IntersectSegmentCapsule(sc, sc + d, Corner(min, max, v), Corner(min, max, v ^ 1), sr, t)) {
            tmin = std::min(t, tmin);
        }
        if (IntersectSegmentCapsule(sc, sc + d, Corner(min, max, v), Corner(min, max, v ^ 2), sr, t)) {
            tmin = std::min(t, tmin);
        }
        if (IntersectSegmentCapsule(sc, sc + d, Corner(min, max, v), Corner(min, max, v ^ 4), sr, t)) {
            tmin = std::min(t, tmin);
        }
        if (tmin == FLT_MAX) return 0; // No intersection
        t = tmin;
        normal = (sc + d * t - p).Unit();
        return 1; // Intersection at time t == tmin
    }
    // If only one bit set in m, then p is in a face region
    if ((m & (m - 1)) == 0) {
        // Do nothing. Time t from intersection with
        // expanded box is correct intersection time
        normal = (sc + d * t - p).Unit();
        return 1;
    }
    // p is in an edge region. Intersect against the capsule at the edge
    int result = IntersectSegmentCapsule(sc, sc + d, Corner(min, max, u ^ 7), Corner(min, max, v), sr, t);
    // TODO: Check if this normal is correct even?
    if (result) normal = (sc + d * t - p).Unit();
    return result;
}
// @return Struct containing collision information about the sweep.
static CollisionManifold SweepCircleVsAABB(const V2_double& origin, const double& radius, const V2_double& velocity, const V2_double& target_position, const V2_double& target_size) {
    //auto c = collision::CircleVsAABB(0, target_position, target_size, {}, origin, radius, velocity);
    CollisionManifold c;
    //auto pair = collision::internal::CircleAABBSweep(target_position, target_size, origin, origin + velocity, radius);
    //c = pair.first;
    // TODO: Figure out this function and what it returns / should return.
    c.occurs = IntersectMovingSphereAABB(origin, radius, velocity, target_position, target_size, c.time, c.normal);

    //c.point = pair.second;
    c.occurs = c.occurs && c.time < 1.0 && (c.time > 0.0 || math::Compare(c.time, 0.0)) && !c.normal.IsZero();
    //debug::PrintLine(c.time);
    return c;
}

} // namespace internal

template <typename T, typename S, typename U>
void Sweep(const V2_double& position, const S& size, V2_double& velocity, const std::vector<V2_double>& target_positions, const std::vector<U>& target_sizes, const std::vector<V2_double>& target_velocities, T lambda) {
    std::vector<CollisionManifold> collisions;
    V2_double final_velocity;
    if (!velocity.IsZero()) {
        assert(target_positions.size() == target_sizes.size());
        bool use_relative_velocity{ target_velocities.size() == target_positions.size() };
        for (std::size_t i = 0; i < target_positions.size(); ++i) {
            V2_double relative_velocity = velocity;
            if (use_relative_velocity) relative_velocity -= target_velocities[i];
            const CollisionManifold collision = lambda(position, size, relative_velocity, target_positions[i], target_sizes[i]);
            if (collision.occurs) {
                collisions.push_back(collision);
            }
        }
        internal::SortCollisionTimes(collisions);
        if (collisions.size() > 0) {
            V2_double new_velocity = internal::GetNewVelocity(velocity, collisions[0]);
            final_velocity += velocity * collisions[0].time;
            // Potential alternative solution to corner clipping:
            //new_origin = origin + (velocity * collisions[0].time - velocity.Unit() * epsilon);
            std::vector<CollisionManifold> collisions2;
            V2_double new_position = position + final_velocity;
            if (!new_velocity.IsZero()) {
                for (std::size_t i = 0; i < target_positions.size(); ++i) {
                    V2_double relative_velocity = new_velocity;
                    if (use_relative_velocity) relative_velocity -= target_velocities[i];
                    const CollisionManifold collision = lambda(new_position, size, new_velocity, target_positions[i], target_sizes[i]);
                    if (collision.occurs) {
                        collisions2.push_back(collision);
                    }
                }
                internal::SortCollisionTimes(collisions2);
                if (collisions2.size() > 0) {
                    final_velocity += new_velocity * collisions2[0].time;
                } else {
                    final_velocity += new_velocity;
                }
            }
        } else {
            final_velocity += velocity;
        }
    }
    velocity = final_velocity;
}



namespace internal {

using CollisionCallback = Manifold(*)(const component::Transform& A,
									  const component::Transform& B,
									  const component::Shape& a,
									  const component::Shape& b);

extern CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::physics::ShapeType::COUNT)];

Manifold StaticAABBvsAABB(const component::Transform& A,
						  const component::Transform& B,
						  const component::Shape& a,
						  const component::Shape& b);

Manifold StaticCirclevsCircle(const component::Transform& A,
							  const component::Transform& B,
							  const component::Shape& a,
							  const component::Shape& b);

Manifold StaticAABBvsCircle(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

Manifold StaticCirclevsAABB(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

} // namespace internal

Manifold StaticIntersection(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

void Clear(ecs::Manager& manager);

void Update(ecs::Manager& manager, double dt);

void Resolve(ecs::Manager& manager);

// Check if two AABBs overlap.
bool AABBvsAABB(const component::Transform& A,
				const component::Transform& B,
				const component::Shape& a,
				const component::Shape& b);

} // namespace collision

} // namespace ptgn

inline std::ostream& operator<<(std::ostream& os, const ptgn::collision::CollisionManifold& obj) {
    os << "Point: " << obj.point << ", Normal: " << obj.normal << ", Time: " << obj.time;
    return os;
}