#pragma once

#include "vector2.h"
#include "polygon.h"
#include "circle.h"
#include "line.h"

namespace ptgn {

namespace impl {

[[nodiscard]] float SquareDistancePointRectangle(const Point<float>& a,
                                   const Rectangle<float>& b);

[[nodiscard]] float ParallelogramArea(const Point<float>& a,
						const Point<float>& b,
						const Point<float>& c);

} // namespace impl

namespace overlap {

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 79.
[[nodiscard]] bool RectangleRectangle(const Rectangle<float>& a,
					    const Rectangle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 88.
[[nodiscard]] bool CircleCircle(const Circle<float>& a,
				  const Circle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 165-166.
[[nodiscard]] bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b);

[[nodiscard]] bool PointRectangle(const Point<float>& a,
					const Rectangle<float>& b);

[[nodiscard]] bool PointCircle(const Point<float>& a,
				 const Circle<float>& b);

// Source: http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
// Page 130. (SqDistPointSegment == 0) but optimized.
[[nodiscard]] bool PointSegment(const Point<float>& a,
                  const Segment<float>& b);

[[nodiscard]] bool SegmentRectangle(const Segment<float>& a,
				      const Rectangle<float>& b);

// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection
[[nodiscard]] bool SegmentCircle(const Segment<float>& a,
			       const Circle<float>& b);

// Source: https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
[[nodiscard]] bool SegmentSegment(const Segment<float>& a,
			        const Segment<float>& b);

} // namespace overlap

namespace intersect {

struct Collision {
	float depth{ 0.0f };
	V2_float normal{ 0.0f, 0.0f };
};

[[nodiscard]] bool RectangleRectangle(const Rectangle<float>& a,
						const Rectangle<float>& b,
						Collision& c);

[[nodiscard]] bool CircleCircle(const Circle<float>& a,
				  const Circle<float>& b,
				  Collision& c);

// Source: https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
[[nodiscard]] bool CircleRectangle(const Circle<float>& a,
					 const Rectangle<float>& b,
					 Collision& c);

} // namespace intersect

namespace dynamic {

struct Collision {
	float t{ 1.0f };       // time of impact.
	Vector2<float> normal; // normal of impact (normalised).
};

// Source: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282
[[nodiscard]] bool SegmentSegment(const Segment<float>& a,
                    const Segment<float>& b,
                    Collision& c);

// Source: https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899
[[nodiscard]] bool SegmentCircle(const Segment<float>& a,
                   const Circle<float>& b,
                   Collision& c);

[[nodiscard]] bool SegmentRectangle(const Segment<float>& a,
                      const Rectangle<float>& b,
                      Collision& c);

// Source: https://stackoverflow.com/a/52462458
[[nodiscard]] bool SegmentCapsule(const Segment<float>& a,
                    const Capsule<float>& b,
                    Collision& c);

// Velocity is taken relative to a, b is seen as static.
[[nodiscard]] bool CircleCircle(const Circle<float>& a,
                  const Vector2<float>& vel,
                  const Circle<float>& b,
                  Collision& c);

// Velocity is taken relative to a, b is seen as static.
[[nodiscard]] bool CircleRectangle(const Circle<float>& a,
                     const Vector2<float>& vel,
                     const Rectangle<float>& b,
                     Collision& c);

// Velocity is taken relative to a, b is seen as static.
[[nodiscard]] bool RectangleRectangle(const Rectangle<float>& a,
                        const Vector2<float>& vel,
                        const Rectangle<float>& b,
                        Collision& c);

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
        PTGN_CHECK(target_positions.size() == target_sizes.size());
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
            //draw::Line(position, new_position, color::Blue);
            //draw::Circle(new_position, size, color::Blue);
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
                    //draw::Line(new_position, new_position + new_velocity * collisions2[0].time, color::Red);
                    //draw::Circle(new_position + new_velocity * collisions2[0].time, size, color::Red);
                    //draw::Circle(position + final_velocity, size, color::Red);
                } else {
                    final_velocity += new_velocity;
                    //draw::Line(new_position, new_position + new_velocity, color::Red);
                    //draw::Circle(new_position + new_velocity, size, color::Red);
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