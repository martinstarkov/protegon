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

#include "debugging/Debug.h" // TODO: TEMPORARY

namespace ptgn {

namespace collision {

struct CollisionManifold {
    CollisionManifold() = default;
    CollisionManifold(const V2_double& point, const V2_double& normal, double time) : point{ point }, normal{ normal }, time{ time } {}
    V2_double point;
    V2_double normal;
    double time = 0.0;
    int intersection = 0;
};

struct Collision {
    ecs::Entity entity;
    CollisionManifold manifold;
};

// Sort times in order of lowest time first, if times are equal prioritize diagonal collisions first.
inline void SortTimes(std::vector<Collision>& collisions) {
    std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
        if (a.manifold.time != b.manifold.time) {
            return a.manifold.time < b.manifold.time;
        } else {
            return a.manifold.normal.MagnitudeSquared() < b.manifold.normal.MagnitudeSquared();
        }
    });
}

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
inline bool RayVsAABB(const V2_double& ray_origin, const V2_double& ray_dir, const V2_double& position, const V2_double& size, CollisionManifold& out_collision) {

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
    if (t_near.x > t_near.y) { // X-axis.
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
    } else if (t_near.x == t_near.y && t_far.x == t_far.y) { // Both axes collide at the same time.
        // Diagonal collision, set normal to opposite of direction of movement.
        // TODO: This may be important to fix corner collisions in the future.
        //out_collision.normal = ray_dir.Identity().Opposite();
    }

    // Raycast collision occurred.
    return true;
}
// Determine the time at which a dynamic AABB would collide with a static AABB.
inline bool DynamicAABBVsAABB(double dt, const V2_double& velocity, const V2_double& dynamic_position, const V2_double& dynamic_size, const V2_double& static_position, const V2_double& static_size, CollisionManifold& collision) {

    // Check if dynamic object has a non-zero velocity. It cannot collide if it is not moving.
    if (velocity.IsZero()) {
        return false;
    }

    // Expand static target by dynamic object dimensions so that only the center of the dynamic object needs to be considered.
    V2_double expanded_position{ static_position - dynamic_size / 2.0 };
    V2_double expanded_size{ static_size + dynamic_size };

    // Check if the velocity ray collides with the expanded target.
    if (RayVsAABB(dynamic_position + dynamic_size / 2, velocity * dt, expanded_position, expanded_size, collision)) {
        return collision.time >= 0.0 && collision.time < 1.0;
    } else {
        return false;
    }
}
// Modify the velocity of a dynamic AABB so it does not collide with a static AABB.
inline bool ResolveDynamicAABBVsAABB(double dt, const V2_double& relative_velocity, V2_double& dynamic_velocity, const V2_double& dynamic_position, const V2_double& dynamic_size, const V2_double& static_position, const V2_double& static_size, const CollisionManifold& collision) {
    CollisionManifold repeat_check;
    // Repeat check is needed due to the fact that if multiple collisions are found, resolving the velocity for the nearest one may invalidate the previously thought collisions.
    if (DynamicAABBVsAABB(dt, relative_velocity, dynamic_position, dynamic_size, static_position, static_size, repeat_check)) {
        dynamic_velocity += collision.normal * math::Abs(dynamic_velocity) * (1.0 - collision.time);
        /*if (std::abs(velocity.x) < LOWEST_VELOCITY) {
            velocity.x = 0.0;
        }
        if (std::abs(velocity.y) < LOWEST_VELOCITY) {
            velocity.y = 0.0;
        }*/
        return true;
    }
    return false;
}
/*
std::pair<CollisionManifold, V2_double> CircleVsAABB(const V2_double& rectangle_position, const V2_double& rectangle_size, const V2_double& start, const V2_double& end, double radius) {
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
        debug::PrintLine("Exiting 1");
        return {};
    }

    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto invdx = (dx == 0.0 ? 0.0 : 1.0 / dx);
    const auto invdy = (dy == 0.0 ? 0.0 : 1.0 / dy);
    V2_double corner = V2_double::Maximum();

    // Calculate intersection times with each side's plane
    // Check each side's quadrant for single-side intersection
    // Calculate Corner

    // Left Side
    // Does the circle go from the left side to the right side of the rectangle's left?
    if (start.x - radius < L && end.x + radius > L) {
        auto ltime = ((L - radius) - start.x) * invdx;
        if (ltime >= 0.0 && ltime <= 1.0) {
            auto ly = dy * ltime + start.y;
            // Does the collisions point lie on the left side?
            if (ly >= T && ly <= B) {
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
        if (rtime >= 0.0 && rtime <= 1.0) {
            auto ry = dy * rtime + start.y;
            // Does the collisions point lie on the right side?
            if (ry >= T && ry <= B) {
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
        if (ttime >= 0.0 && ttime <= 1.0) {
            auto tx = dx * ttime + start.x;
            // Does the collisions point lie on the top side?
            if (tx >= L && tx <= R) {
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
        if (btime >= 0.0 && btime <= 1.0) {
            auto bx = dx * btime + start.x;
            // Does the collisions point lie on the bottom side?
            if (bx >= L && bx <= R) {
                // circle center: { bx, dy * btime + start.y }
                return { CollisionManifold({ bx, B }, { 0, 1 }, btime), { bx, dy * btime + start.y } };
            }
        }
        corner.y = B;
    }

    // No intersection at all!
    if (corner.x == V2_double::Maximum().x && corner.y == V2_double::Maximum().y) {
        debug::PrintLine("Exiting 2");
        return {};
    }

    // Account for the times where we don't pass over a side but we do hit it's corner
    if (corner.x != V2_double::Maximum().x && corner.y == V2_double::Maximum().y) {
        corner.y = (dy > 0.0 ? B : T);
    }
    if (corner.y != V2_double::Maximum().y && corner.x == V2_double::Maximum().x) {
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

    // If the circle is too close, no intersection.
    if (cornerDistance < radius) {
        debug::PrintLine("Exiting 3");
        return {};
    }

    // If inner angle is zero, it's going to hit the corner straight on.
    if (innerAngle == 0.0) {
        double time = (double)((cornerDistance - radius) / lineLength);

        // If time is outside the boundaries, return null. This algorithm can
        // return a negative time which indicates a previous intersection, and
        // can also return a time > 1.0 which can predict a corner intersection.
        if (time > 1.0 || time < 0.0) {
            debug::PrintLine("Exiting 4");
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
        debug::PrintLine("Exiting 5: ", innerAngleSin, ",", cornerDistance, ",", inverseRadius);
        return {};
    }

    double angle1 = math::PI<double> - std::asin(angle1Sin);
    double angle2 = math::PI<double> - innerAngle - angle1;
    double intersectionDistance = radius * std::sin(angle2) / innerAngleSin;

    // Solve for time
    double time = (double)(intersectionDistance / lineLength);

    // If time is outside the boundaries, return null. This algorithm can
    // return a negative time which indicates a previous intersection, and
    // can also return a time > 1.0f which can predict a corner intersection.
    if (time > 1.0 || time < 0.0) {
        debug::PrintLine("Exiting 6");
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
*/

void IntersectsVertex(int32_t i0, int32_t i1, V2_double const& K,
                      double q0, double q1, double q2, CollisionManifold& result) {
    result.intersection = +1;
    result.time = (q0 - std::sqrt(q1)) / q2;
    result.point[i0] = K[i0];
    result.point[i1] = K[i1];
}

void IntersectsEdge(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
                    double radius, V2_double const& V, CollisionManifold& result) {
    result.intersection = +1;
    result.time = (K0[i0] + radius - C[i0]) / V[i0];
    result.point[i0] = K0[i0];
    result.point[i1] = C[i1] + result.time * V[i1];
}


void InteriorOverlap(V2_double const& C, CollisionManifold& result) {
    result.intersection = -1;
    result.time = (double)0;
    result.point = C;
}

void EdgeOverlap(int32_t i0, int32_t i1, V2_double const& K, V2_double const& C,
                 V2_double const& delta, double radius, CollisionManifold& result) {
    result.intersection = (delta[i0] < radius ? -1 : 1);
    result.time = (double)0;
    result.point[i0] = K[i0];
    result.point[i1] = C[i1];
}

void VertexOverlap(V2_double const& K0, V2_double const& delta,
                   double radius, CollisionManifold& result) {
    double sqrDistance = delta[0] * delta[0] + delta[1] * delta[1];
    double sqrRadius = radius * radius;
    result.intersection = (sqrDistance < sqrRadius ? -1 : 1);
    result.time = (double)0;
    result.point = K0;
}

void VertexSeparated(V2_double const& K0, V2_double const& delta0,
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

void EdgeUnbounded(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
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

void VertexUnbounded(V2_double const& K0, V2_double const& C, double radius,
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
void DoQuery(V2_double const& K, V2_double const& C,
             double radius, V2_double const& V, CollisionManifold& result) {
    V2_double delta = C - K;
    if (delta[1] <= radius) {
        if (delta[0] <= radius) {
            if (delta[1] <= (double)0) {
                if (delta[0] <= (double)0) {
                    InteriorOverlap(C, result);
                } else {
                    EdgeOverlap(0, 1, K, C, delta, radius, result);
                }
            } else {
                if (delta[0] <= (double)0) {
                    EdgeOverlap(1, 0, K, C, delta, radius, result);
                } else {
                    if (delta.DotProduct(delta) <= radius * radius) {
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
        if (delta[0] <= radius) {
            EdgeUnbounded(1, 0, K, C, radius, delta, V, result);
        } else {
            VertexUnbounded(K, C, radius, delta, V, result);
        }
    }
}

CollisionManifold CircleVsAABB(double dt, const V2_double& box_position, const V2_double& box_size, V2_double const& boxVelocity,
                    const V2_double& circle_center, double circle_radius, V2_double const& circleVelocity) {
    CollisionManifold result{};

    // Translate the circle and box so that the box center becomes
    // the origin.  Compute the velocity of the circle relative to
    // the box.

    auto box_min = box_position;
    auto box_max = box_position + box_size;

    V2_double extent = box_size / 2;
    V2_double boxCenter = box_position + extent;
    V2_double C = circle_center - boxCenter;
    V2_double V = (circleVelocity - boxVelocity) * dt;

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

    if (result.intersection != 0) {
        // Translate back to the original coordinate system.
        for (int32_t i = 0; i < 2; ++i) {
            if (sign[i] < (double)0) {
                result.point[i] = -result.point[i];
            }
        }

        result.point += boxCenter;

        result.normal = (result.point - circle_center).Unit().Opposite();
    }
    return result;
}

namespace internal {

using CollisionCallback = Manifold(*)(const component::Transform& A,
									  const component::Transform& B,
									  const component::Shape& a,
									  const component::Shape& b);

extern CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)];

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