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

struct Collision {
	float t{ 1.0f };       // time of impact.
	Vector2<float> normal; // normal of impact (normalised).
};

int IntersectMovingCircleRectangle(Segment<float> seg, float r, const Rectangle<float> b, Collision& col);
int IntersectMovingCircleCircle(Segment<float> seg, float r, const Circle<float> b, Collision& col);
int IntersectMovingRectangleRectangle(Segment<float> seg, V2_float size, const Rectangle<float> b, Collision& col);

/*
namespace impl {

template <typename T>
class CollisionQuery {
public:
    // Currently, only a dynamic query is supported.  A static query will
    // need to compute the intersection set of (solid) box and circle.
    struct Result {
        Result()
            :
            intersectionType(0),
            contactTime(static_cast<T>(0)),
            contactPoint(V2_double::Zero()) {}

        // The cases are
        // 1. Objects initially overlapping.  The contactPoint is only one
        //    of infinitely many points in the overlap.
        //      intersectionType = -1
        //      contactTime = 0
        //      contactPoint = circle.center
        // 2. Objects initially separated but do not intersect later.  The
        //      contactTime and contactPoint are invalid.
        //      intersectionType = 0
        //      contactTime = 0
        //      contactPoint = (0,0)
        // 3. Objects initially separated but intersect later.
        //      intersectionType = +1
        //      contactTime = first time T > 0
        //      contactPoint = corresponding first contact
        int32_t intersectionType;
        T contactTime;
        V2_double contactPoint;

        // TODO: To support arbitrary precision for the contactTime,
        // return q0, q1 and q2 where contactTime = (q0 - sqrt(q1)) / q2.
        // The caller can compute contactTime to desired number of digits
        // of precision.  These are valid when intersectionType is +1 but
        // are set to zero (invalid) in the other cases.  Do the same for
        // the contactPoint.
    };

    Result operator()(const V2_double& boxmin, const V2_double& boxmax, const V2_double& boxVelocity,
                      const V2_double& circlecenter, const double& circleradius, const V2_double& circleVelocity, bool circle = false) {
        Result result{};

        // Translate the circle and box so that the box center becomes
        // the origin.  Compute the velocity of the circle relative to
        // the box.
        V2_double boxCenter = (boxmax + boxmin) * (T)0.5;
        V2_double extent = (boxmax - boxmin) * (T)0.5;
        V2_double C = circlecenter - boxCenter;
        V2_double V = circleVelocity - boxVelocity;

        // Change signs on components, if necessary, to transform C to the
        // first quadrant.  Adjust the velocity accordingly.
        std::array<T, 2> sign = { (T)0, (T)0 };

        for (int32_t i = 0; i < 2; ++i) {
            if (C[i] > (T)0 || math::Compare(C[i], 0.0)) {
                sign[i] = (T)1;
            } else {
                C[i] = -C[i];
                V[i] = -V[i];
                sign[i] = (T)-1;
            }
        }

        DoQuery(extent, C, circleradius, V, result, circle);

        if (result.intersectionType != 0) {
            // Translate back to the original coordinate system.
            for (int32_t i = 0; i < 2; ++i) {
                if (sign[i] < (T)0) {
                    result.contactPoint[i] = -result.contactPoint[i];
                }
            }

            result.contactPoint += boxCenter;
        }
        return result;
    }

protected:
    void DoQuery(V2_double const& K, V2_double const& C,
                 T radius, V2_double const& V, Result& result, bool circle) {
        V2_double delta = C - K;
        if (delta[1] < radius) {
            if (delta[0] < radius) {
                if (delta[1] < (T)0 || math::Compare(delta[1], 0.0)) {
                    if (delta[0] < (T)0 || math::Compare(delta[0], 0.0)) {
                        InteriorOverlap(C, result);
                    } else {
                        if (!circle)
                            EdgeOverlap(0, 1, K, C, delta, radius, result);
                    }
                } else {
                    if (delta[0] < (T)0 || math::Compare(delta[0], 0.0)) {
                        if (!circle)
                            EdgeOverlap(1, 0, K, C, delta, radius, result);
                    } else {
                        if (delta.DotProduct(delta) < radius * radius) {
                            VertexOverlap(K, delta, radius, result, V);
                        } else {
                            VertexSeparated(K, delta, V, radius, result);
                        }
                    }

                }
            } else {
                EdgeUnbounded(0, 1, K, C, radius, delta, V, result, circle);
            }
        } else {
            if (delta[0] < radius) {
                EdgeUnbounded(1, 0, K, C, radius, delta, V, result, circle);
            } else {
                VertexUnbounded(K, C, radius, delta, V, result, circle);
            }
        }
    }

private:
    void InteriorOverlap(const V2_double& C, Result& result) {
        result.intersectionType = -1;
        result.contactTime = (T)0;
        result.contactPoint = C;
    }

    void EdgeOverlap(int32_t i0, int32_t i1, V2_double const& K, V2_double const& C,
                     V2_double const& delta, T radius, Result& result) {
        result.intersectionType = (delta[i0] < radius ? -1 : 1);
        result.contactTime = (T)0;
        result.contactPoint[i0] = K[i0];
        result.contactPoint[i1] = C[i1];
    }

    void VertexOverlap(V2_double const& K0, V2_double const& delta,
                       T radius, Result& result, const V2_double& V) {
        T sqrDistance = delta[0] * delta[0] + delta[1] * delta[1];
        T sqrRadius = radius * radius;
        auto delta0 = delta;
        if (math::Compare(sqrDistance, sqrRadius, 1e-5)) {
            // corner collision
            VertexSeparated(K0, delta, V, radius, result);
        } else {
            result.intersectionType = (sqrDistance < sqrRadius ? -1 : 1);
            result.contactTime = (T)0;
            result.contactPoint = K0;
        }
    }

    void VertexSeparated(V2_double const& K0, V2_double const& delta0,
                         V2_double const& V, T radius, Result& result) {
        T q0 = -V.DotProduct(delta0);
        if (q0 > (T)0) {
            T dotVPerpD0 = V.DotProduct(delta0.Tangent());
            T q2 = V.DotProduct(V);
            T q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
            if (q1 > (T)0) {
                IntersectsVertex(0, 1, K0, q0, q1, q2, result);
            }
        }
    }

    void EdgeUnbounded(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
                       T radius, V2_double const& delta0, V2_double const& V, Result& result, bool circle) {
        if (V[i0] < (T)0) {
            T dotVPerpD0 = V[i0] * delta0[i1] - V[i1] * delta0[i0];
            if (radius * V[i1] + dotVPerpD0 > (T)0) {
                V2_double K1, delta1;
                K1[i0] = K0[i0];
                K1[i1] = -K0[i1];
                delta1[i0] = C[i0] - K1[i0];
                delta1[i1] = C[i1] - K1[i1];
                T dotVPerpD1 = V[i0] * delta1[i1] - V[i1] * delta1[i0];
                if (radius * V[i1] + dotVPerpD1 < (T)0) {
                    IntersectsEdge(i0, i1, K0, C, radius, V, result);
                } else {
                    T q2 = V.DotProduct(V);
                    T q1 = radius * radius * q2 - dotVPerpD1 * dotVPerpD1;
                    if (q1 >= (T)0) {
                        T q0 = -(V[i0] * delta1[i0] + V[i1] * delta1[i1]);
                        IntersectsVertex(i0, i1, K1, q0, q1, q2, result);
                    }
                }
            } else {
                T q2 = V.DotProduct(V);
                T q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
                if (q1 > (T)0) {
                    T q0 = -(V[i0] * delta0[i0] + V[i1] * delta0[i1]);
                    IntersectsVertex(i0, i1, K0, q0, q1, q2, result);
                }
            }
        }
    }

    void VertexUnbounded(V2_double const& K0, V2_double const& C, T radius,
                         V2_double const& delta0, V2_double const& V, Result& result, bool circle) {
        if (V[0] < (T)0 && V[1] < (T)0) {
            T dotVPerpD0 = V.DotProduct(delta0.Tangent());
            if (radius * V[0] - dotVPerpD0 < (T)0) {
                if (-radius * V[1] - dotVPerpD0 > (T)0) {
                    T q2 = V.DotProduct(V);
                    T q1 = radius * radius * q2 - dotVPerpD0 * dotVPerpD0;
                    T q0 = -V.DotProduct(delta0);
                    IntersectsVertex(0, 1, K0, q0, q1, q2, result);
                } else {
                    V2_double K1{ K0[0], -K0[1] };
                    V2_double delta1 = C - K1;
                    T dotVPerpD1 = V.DotProduct(delta1.Tangent());
                    if (-radius * V[1] - dotVPerpD1 > (T)0) {
                        if (!circle)
                            IntersectsEdge(0, 1, K0, C, radius, V, result);
                    } else {
                        T q2 = V.DotProduct(V);
                        T q1 = radius * radius * q2 - dotVPerpD1 * dotVPerpD1;
                        if (q1 > (T)0) {
                            T q0 = -V.DotProduct(delta1);
                            IntersectsVertex(0, 1, K1, q0, q1, q2, result);
                        }
                    }
                }
            } else {
                V2_double K2{ -K0[0], K0[1] };
                V2_double delta2 = C - K2;
                T dotVPerpD2 = V.DotProduct(delta2.Tangent());
                if (radius * V[0] - dotVPerpD2 < (T)0) {
                    if (!circle)
                        IntersectsEdge(1, 0, K0, C, radius, V, result);
                } else {
                    T q2 = V.DotProduct(V);
                    T q1 = radius * radius * q2 - dotVPerpD2 * dotVPerpD2;
                    if (q1 > (T)0) {
                        T q0 = -V.DotProduct(delta2);
                        IntersectsVertex(1, 0, K2, q0, q1, q2, result);
                    }
                }
            }
        }
    }

    void IntersectsVertex(int32_t i0, int32_t i1, V2_double const& K,
                          T q0, T q1, T q2, Result& result) {
        result.intersectionType = +1;
        result.contactTime = (q0 - std::sqrt(q1)) / q2;
        result.contactPoint[i0] = K[i0];
        result.contactPoint[i1] = K[i1];
    }

    void IntersectsEdge(int32_t i0, int32_t i1, V2_double const& K0, V2_double const& C,
                        T radius, V2_double const& V, Result& result) {
        result.intersectionType = +1;
        result.contactTime = (K0[i0] + radius - C[i0]) / V[i0];
        result.contactPoint[i0] = K0[i0];
        result.contactPoint[i1] = C[i1] + result.contactTime * V[i1];
    }
};

} // namespace impl
*/

/*
// Return true if r1 and r2 are real
inline bool QuadraticFormula(const double a, const double b, const double c,
                             double& r1, double& r2) { // first and second roots.
    const double q = b * b - 4 * a * c;
    if (q < 0 || math::Compare(q, 0.0, std::numeric_limits<double>::epsilon())) {
        r1 = r2 = 1.0;
        return false; // complex or repeated roots.
    } else {
        const double sq = std::sqrt(q);
        const double d = 0.5 / a;
        r1 = (-b + sq) * d;
        r2 = (-b - sq) * d;
        return true; // real roots.
    }
}
*/

/*
inline bool RayVsRectangle(const V2_double& ray_origin, const V2_double& ray_dir,
                           const V2_double& position, const V2_double& size,
                           CollisionManifold& out_collision) {

    // Initial condition: no collision normal.
    out_collision.normal = { 0.0, 0.0 };
    out_collision.point = { 0.0, 0.0 };

    // Cache division.
    V2_double inv_dir = 1.0 / ray_dir;

    // Calculate intersections with rectangle bounding axes.
    V2_double t_near = (position - ray_origin) * inv_dir;
    V2_double t_far = (position + size - ray_origin) * inv_dir;

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
*/

/*
// @return Struct containing collision information about the sweep.
Collision DynamicRectangleVsRectangle(const V2_double& position,
                                              const V2_double& size,
                                              const V2_double& target_position,
                                              const V2_double& target_size,
                                              const V2_double& velocity) {
    collision::CollisionManifold c;
    V2_double expanded_position{ target_position - target_size / 2.0 };
    V2_double expanded_size{ target_size + size };
    bool occured = collision::internal::RayVsRectangle(position + size / 2, velocity, expanded_position, expanded_size, c);
    c.occurs = occured && c.time < 1.0 && (c.time > 0.0 || math::Compare(c.time, 0.0)) && !c.normal.IsZero();
    if (c.occurs) c.distance = (position + size / 2 - (target_position + target_size / 2)).MagnitudeSquared();
    return c;
}
*/

// TODO: This function is not clip proof. Figure out why the clipping occurs:
// HINT: It most likely occurs due to the quadratic formula solution being 
// within 1e-10 of 0.0 due to floating point error.
// UPDATE: Potentially fixed? I tried testing for 10 minutes and could not
// see it occuring so perhaps making the epsilon be std::numeric_limits or
// removing the compare u0 u1 which set t=1.0 did the trick. Keeping this
// comment here in case this clipping arises again in the future.
// @return Struct containing collision information about the sweep.
/*
Collision DynamicCircleVsCircle(const V2_double& position,
                                        const V2_double& size,
                                        const V2_double& target_position,
                                        const V2_double& target_size,
                                        const V2_double& velocity) {

    const double radius{ size.x };
    const double target_radius{ target_size.x };
    double u0, u1;
    const V2_double AB = target_position - position; // vector from A0 to B0
    const double rab = radius + target_radius; // combined radius
    const auto rel_vel = -velocity;
    const double a = rel_vel.DotProduct(rel_vel); //u*u coefficient
    const double b = 2 * rel_vel.DotProduct(AB); //u coefficient
    const double c = AB.DotProduct(AB) - rab * rab;

    collision::CollisionManifold collision;

    // TODO: Figure out how to reintroduce this without causing sticking.
    // constant term, check if they're currently overlapping.
    //if (AB.DotProduct(AB) <= rab * rab) {
    //    u0 = 0;
    //    u1 = 0;
    //    auto new_origin = position + rel_vel * u0;
    //    collision::CollisionManifold collision;
    //    collision.occurs = true;
    //    collision.distance = (position - target_position).MagnitudeSquared();
    //    collision.normal = (new_origin - target_position).Unit();
    //    collision.time = u0;
    //    collision.point = new_origin;
    //    // TODO: Put an else here and connect to the below if-statement when introducing this
    //}

    // check if they hit each other during the frame.
    if (internal::QuadraticFormula(a, b, c, u0, u1)) {
        if (u0 > u1)
            std::swap(u0, u1);
        // TODO: Check that this is accurate to theory:
        // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
        auto new_origin = position + rel_vel * u0;
        collision.occurs = true;
        collision.distance = (position - target_position).MagnitudeSquared();
        collision.normal = (new_origin - target_position).Unit();
        collision.time = u0;
        collision.point = new_origin;
    } else {
        collision.time = 1.0;
    }
    collision.occurs = collision.occurs && collision.time < 1.0 && (collision.time > 0.0 || math::Compare(collision.time, 0.0)) && !collision.normal.IsZero();
    return collision;
}
*/

/*
Collision DynamicCircleVsRectangle(const V2_double& position,
                                           const V2_double& size,
                                           const V2_double& target_position,
                                           const V2_double& target_size,
                                           const V2_double& velocity) {
    // TODO: Combine the collision query and manifold functions.
    const double radius{ size.x };
    collision::CollisionManifold cnew;
    internal::CollisionQuery<double> query;
    auto c = query(target_position, target_position + target_size, V2_double{}, position, radius, velocity);
    V2_double normal = (position + velocity * c.contactTime - c.contactPoint).Unit();
    if (c.intersectionType == 1) {
        if ((c.contactTime < 1.0 || math::Compare(c.contactTime, 1.0)) && (c.contactTime > 0.0 || math::Compare(c.contactTime, 0.0))) {
            cnew.normal = normal;
            cnew.time = c.contactTime;
            cnew.occurs = true;
            cnew.point = c.contactPoint;
        }
    } else if (c.intersectionType == 0) {
        cnew.occurs = false;
        cnew.time = 1.0;
    } else if (c.intersectionType == -1) {
        if (cnew.normal.IsZero() && math::Compare(c.contactTime, 0.0)) {
            cnew = {};
            V2_double expanded_position{ target_position - radius };
            V2_double expanded_size{ target_size + radius * 2 };
            bool occured = collision::internal::RayVsRectangle(position, velocity, expanded_position, expanded_size, cnew);
            cnew.occurs = occured && cnew.time < 1.0 && (cnew.time > 0.0 || math::Compare(cnew.time, 0.0)) && !cnew.normal.IsZero();
            if (cnew.occurs) cnew.distance = (position - (target_position + target_size / 2)).MagnitudeSquared();
        } else {
            cnew.normal = normal;
            cnew.time = c.contactTime;
            cnew.occurs = true;
            cnew.point = c.contactPoint;
        }
    }
    return cnew;
}
*/

/*
Collision DynamicRectangleVsCircle(const V2_double& position,
                                   const V2_double& size,
                                   const V2_double& target_position,
                                   const V2_double& target_size,
                                   const V2_double& velocity) {
    return DynamicCircleVsRectangle(target_position, target_size, position, size, -velocity);
}
*/

/*
template <typename T>
void Sweep(const V2_double& position,
           const V2_double& size,
           V2_double& out_velocity,
           const std::vector<V2_double>& target_positions,
           const std::vector<V2_double>& target_sizes,
           const std::vector<V2_double>& target_velocities,
           T lambda) {
    std::vector<CollisionManifold> collisions;
    V2_double final_velocity;
    if (!out_velocity.IsZero()) {
        assert(target_positions.size() == target_sizes.size());
        bool use_relative_velocity{ target_velocities.size() == target_positions.size() };
        for (std::size_t i = 0; i < target_positions.size(); ++i) {
            V2_double relative_velocity = out_velocity;
            if (use_relative_velocity) relative_velocity -= target_velocities[i];
            const CollisionManifold collision = lambda(position, size, target_positions[i], target_sizes[i], relative_velocity);
            if (collision.occurs) {
                collisions.push_back(collision);
            }
        }
        internal::SortCollisionTimes(collisions);
        if (collisions.size() > 0) {
            V2_double new_velocity = internal::GetNewVelocity(out_velocity, collisions[0]);
            final_velocity += out_velocity * collisions[0].time;
            // Potential alternative solution to corner clipping:
            //new_origin = origin + (velocity * collisions[0].time - velocity.Unit() * epsilon);
            std::vector<CollisionManifold> collisions2;
            V2_double new_position = position + final_velocity;
            //draw::Line(position, new_position, color::BLUE);
            //draw::Circle(new_position, size, color::BLUE);
            if (!new_velocity.IsZero()) {
                for (std::size_t i = 0; i < target_positions.size(); ++i) {
                    V2_double new_relative_velocity = new_velocity;
                    if (use_relative_velocity) new_relative_velocity -= target_velocities[i];
                    const CollisionManifold collision = lambda(new_position, size, target_positions[i], target_sizes[i], new_relative_velocity);
                    if (collision.occurs) {
                        collisions2.push_back(collision);
                    }
                }
                internal::SortCollisionTimes(collisions2);
                if (collisions2.size() > 0) {
                    final_velocity += new_velocity * collisions2[0].time;
                    //draw::Line(new_position, new_position + new_velocity * collisions2[0].time, color::RED);
                    //draw::Circle(new_position + new_velocity * collisions2[0].time, size, color::RED);
                    //draw::Circle(position + final_velocity, size, color::RED);
                } else {
                    final_velocity += new_velocity;
                    //draw::Line(new_position, new_position + new_velocity, color::RED);
                    //draw::Circle(new_position + new_velocity, size, color::RED);
                }
            }
        } else {
            final_velocity += out_velocity;
        }
    }
    out_velocity = final_velocity;
}
*/

} // namespace dynamic

} // namespace ptgn