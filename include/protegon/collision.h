#pragma once

#include "ecs/ecs.h"
#include "protegon/circle.h"
#include "protegon/line.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"

namespace ptgn {

class Game;

class OverlapCollision {
public:
	OverlapCollision()									 = default;
	~OverlapCollision()									 = default;
	OverlapCollision(const OverlapCollision&)			 = delete;
	OverlapCollision(OverlapCollision&&)				 = default;
	OverlapCollision& operator=(const OverlapCollision&) = delete;
	OverlapCollision& operator=(OverlapCollision&&)		 = default;

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 79.
	[[nodiscard]] static bool RectangleRectangle(
		const Rectangle<float>& a, const Rectangle<float>& b
	);

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 88.
	[[nodiscard]] static bool CircleCircle(const Circle<float>& a, const Circle<float>& b);

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 165-166.
	[[nodiscard]] static bool CircleRectangle(const Circle<float>& a, const Rectangle<float>& b);

	[[nodiscard]] static bool PointRectangle(const Point<float>& a, const Rectangle<float>& b);

	[[nodiscard]] static bool PointCircle(const Point<float>& a, const Circle<float>& b);

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130. (SqDistPointSegment == 0) but optimized.
	[[nodiscard]] static bool PointSegment(const Point<float>& a, const Segment<float>& b);

	[[nodiscard]] static bool SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b);

	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection
	[[nodiscard]] static bool SegmentCircle(const Segment<float>& a, const Circle<float>& b);

	// Source:
	// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
	[[nodiscard]] static bool SegmentSegment(const Segment<float>& a, const Segment<float>& b);

private:
	[[nodiscard]] static float SquareDistancePointRectangle(
		const Point<float>& a, const Rectangle<float>& b
	);

	[[nodiscard]] static float ParallelogramArea(
		const Point<float>& a, const Point<float>& b, const Point<float>& c
	);
};

struct IntersectCollision {
	float depth{ 0.0f };
	V2_float normal{ 0.0f, 0.0f };
};

class IntersectCollisionHandler {
public:
	IntersectCollisionHandler()											   = default;
	~IntersectCollisionHandler()										   = default;
	IntersectCollisionHandler(const IntersectCollisionHandler&)			   = delete;
	IntersectCollisionHandler(IntersectCollisionHandler&&)				   = default;
	IntersectCollisionHandler& operator=(const IntersectCollisionHandler&) = delete;
	IntersectCollisionHandler& operator=(IntersectCollisionHandler&&)	   = default;

	static bool RectangleRectangle(
		const Rectangle<float>& a, const Rectangle<float>& b, IntersectCollision& c
	);

	static bool CircleCircle(const Circle<float>& a, const Circle<float>& b, IntersectCollision& c);

	// Source:
	// https://steamcdn-a.akamaihd.net/apps/valve/2015/DirkGregorius_Contacts.pdf
	static bool CircleRectangle(
		const Circle<float>& a, const Rectangle<float>& b, IntersectCollision& c
	);
};

struct DynamicCollision {
	float t{ 1.0f }; // time of impact.
	V2_float normal; // normal of impact (normalised).
};

enum class DynamicCollisionResponse {
	Slide,
	Bounce,
	Push
};

enum class DynamicCollisionShape {
	Circle,
	Rectangle
};

class DynamicCollisionHandler {
public:
	DynamicCollisionHandler()										   = default;
	~DynamicCollisionHandler()										   = default;
	DynamicCollisionHandler(const DynamicCollisionHandler&)			   = delete;
	DynamicCollisionHandler(DynamicCollisionHandler&&)				   = default;
	DynamicCollisionHandler& operator=(const DynamicCollisionHandler&) = delete;
	DynamicCollisionHandler& operator=(DynamicCollisionHandler&&)	   = default;

	// Source:
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282
	static bool SegmentSegment(
		const Segment<float>& a, const Segment<float>& b, DynamicCollision& c
	);

	// Source:
	// https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899
	static bool SegmentCircle(const Segment<float>& a, const Circle<float>& b, DynamicCollision& c);

	static bool SegmentRectangle(
		const Segment<float>& a, const Rectangle<float>& b, DynamicCollision& c
	);

	// Source: https://stackoverflow.com/a/52462458
	static bool SegmentCapsule(
		const Segment<float>& a, const Capsule<float>& b, DynamicCollision& c
	);

	// Velocity is taken relative to a, b is seen as static.
	static bool CircleCircle(
		const Circle<float>& a, const V2_float& vel, const Circle<float>& b, DynamicCollision& c
	);

	// Velocity is taken relative to a, b is seen as static.
	static bool CircleRectangle(
		const Circle<float>& a, const V2_float& vel, const Rectangle<float>& b, DynamicCollision& c
	);

	// Velocity is taken relative to a, b is seen as static.
	static bool RectangleRectangle(
		const Rectangle<float>& a, const V2_float& vel, const Rectangle<float>& b,
		DynamicCollision& c
	);

	static bool GeneralShape(
		const V2_float& pos1, const V2_float& size1, Origin origin1, DynamicCollisionShape shape1,
		const V2_float& pos2, const V2_float& size2, Origin origin2, DynamicCollisionShape shape2,
		const V2_float& relative_velocity, DynamicCollision& c, float& distance_squared
	);

	template <typename Return>
	using GetCallback = std::function<Return(ecs::Entity)>;

	// The point of the callback functions is so the user can pass targets with more complex
	// components.
	// @return Position that needs to be added to the player position to offset them out of any
	// potential collisions.
	template <typename... Ts>
	static V2_float Sweep(
		float dt, const ecs::Entity& object,
		const ecs::EntityContainer<ecs::LoopCriterion::WithComponents, Ts...>& targets,
		const GetCallback<V2_float>& get_position, const GetCallback<V2_float>& get_size,
		const GetCallback<V2_float>& get_velocity, const GetCallback<Origin>& get_origin,
		const GetCallback<DynamicCollisionShape>& get_shape, DynamicCollisionResponse response
	) {
		const auto v1 = get_velocity(object) * dt;

		if (v1.IsZero()) {
			// TODO: Consider adding a static intersect check here.
			return {};
		}

		const auto p1	  = get_position(object);
		const auto s1	  = get_size(object);
		const auto o1	  = get_origin(object);
		const auto shape1 = get_shape(object);

		DynamicCollision c;

		auto get_sorted_collisions = [&](const auto& pos, const auto& vel) {
			std::vector<SweepCollision> collisions;

			targets.ForEach([&](ecs::Entity e2) {
				if (e2 == object) {
					return;
				}
				auto rel_vel{ vel - get_velocity(e2) * dt };
				float dist2{ 0.0f };
				if (GeneralShape(
						pos, s1, o1, shape1, get_position(e2), get_size(e2), get_origin(e2),
						get_shape(e2), rel_vel, c, dist2
					)) {
					collisions.emplace_back(c, dist2);
				}
			});

			SortCollisions(collisions);
			return collisions;
		};

		auto collisions = get_sorted_collisions(p1, v1);

		if (collisions.size() == 0) { // no collisions occured.
			return {};
		}

		const auto new_velocity = GetRemainingVelocity(v1, collisions[0].c, response);
		auto final_velocity		= v1 * collisions[0].c.t;

		if (new_velocity.IsZero()) {
			return final_velocity - v1;
		}

		// Potential alternative solution to corner clipping:
		// new_origin = origin + (velocity * collisions[0].c.t - velocity.Unit() * epsilon);
		const auto new_p1 = p1 + final_velocity;
		// game.renderer.DrawLine(p1, new_p1, color::Blue);
		// game.renderer.DrawCircleHollow(new_p1, s1, color::Blue);

		auto collisions2 = get_sorted_collisions(new_p1, new_velocity);

		if (collisions2.size() > 0) {
			final_velocity += new_velocity * collisions2[0].c.t;
			// game.renderer.DrawLine(new_p1, new_p1 + new_velocity * collisions2[0].c.t,
			// color::Red);
			// game.renderer.DrawCircleHollow(new_p1 + new_velocity * collisions2[0].c.t, s1,
			// color::Red);
		} else {
			final_velocity += new_velocity;
			// game.renderer.DrawCircleHollow(p1 + new_v1, s1,
			// color::Red);
		}

		return final_velocity - v1;
	}

private:
	struct SweepCollision {
		SweepCollision() = default;

		SweepCollision(const DynamicCollision& c, float dist2) : c{ c }, dist2{ dist2 } {}

		DynamicCollision c;
		float dist2{ 0.0f };
	};

	static void SortCollisions(std::vector<SweepCollision>& collisions);

	[[nodiscard]] static V2_float GetRemainingVelocity(
		const V2_float& velocity, const DynamicCollision& c, DynamicCollisionResponse response
	);
};

class CollisionHandler {
public:
	OverlapCollision overlap{};
	IntersectCollisionHandler intersect{};
	DynamicCollisionHandler dynamic{};

private:
	friend class Game;

	CollisionHandler()									 = default;
	~CollisionHandler()									 = default;
	CollisionHandler(const CollisionHandler&)			 = delete;
	CollisionHandler(CollisionHandler&&)				 = default;
	CollisionHandler& operator=(const CollisionHandler&) = delete;
	CollisionHandler& operator=(CollisionHandler&&)		 = default;

	void Init();
	void Shutdown();
};

} // namespace ptgn