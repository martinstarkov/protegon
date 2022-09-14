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

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b,
					 Collision& c);

} // namespace intersect

namespace dynamic {

struct Collision {
	float t{ 1.0f };       // time of impact.
	Vector2<float> normal; // normal of impact (normalised).
};

// Source: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282
bool SegmentSegment(const Segment<float>& a,
                    const Segment<float>& b,
                    Collision& c);

bool CircleRectangle(const Segment<float>& seg, float r, const Rectangle<float>& b, Collision& col);
bool CircleCircle(const Segment<float>& seg, float r, const Circle<float>& b, Collision& col);
bool RectangleRectangle(const Segment<float>& seg, const V2_float& size, const Rectangle<float>& b, Collision& col);

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