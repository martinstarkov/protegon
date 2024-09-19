#pragma once

#include <functional>
#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "protegon/circle.h"
#include "protegon/line.h"
#include "protegon/log.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"

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

	[[nodiscard]] static bool PointRectangle(const V2_float& a, const Rectangle<float>& b);

	[[nodiscard]] static bool PointCircle(const V2_float& a, const Circle<float>& b);

	// Source:
	// http://www.r-5.org/files/books/computers/algo-list/realtime-3d/Christer_Ericson-Real-Time_Collision_Detection-EN.pdf
	// Page 130. (SqDistPointSegment == 0) but optimized.
	[[nodiscard]] static bool PointSegment(const V2_float& a, const Segment<float>& b);

	[[nodiscard]] static bool SegmentRectangle(const Segment<float>& a, const Rectangle<float>& b);

	// Source: https://www.baeldung.com/cs/circle-line-segment-collision-detection
	[[nodiscard]] static bool SegmentCircle(const Segment<float>& a, const Circle<float>& b);

	// Source:
	// https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/
	[[nodiscard]] static bool SegmentSegment(const Segment<float>& a, const Segment<float>& b);

private:
	[[nodiscard]] static float SquareDistancePointRectangle(
		const V2_float& a, const Rectangle<float>& b
	);

	[[nodiscard]] static float ParallelogramArea(
		const V2_float& a, const V2_float& b, const V2_float& c
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

	// @return Final velocity of the object to prevent them from colliding with the manager objects.
	static V2_float Sweep(
		ecs::Entity entity, ecs::Manager& manager, DynamicCollisionResponse response
	);

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
	OverlapCollision overlap;
	IntersectCollisionHandler intersect;
	DynamicCollisionHandler dynamic;

private:
	friend class Game;

	CollisionHandler()									 = default;
	~CollisionHandler()									 = default;
	CollisionHandler(const CollisionHandler&)			 = delete;
	CollisionHandler(CollisionHandler&&)				 = default;
	CollisionHandler& operator=(const CollisionHandler&) = delete;
	CollisionHandler& operator=(CollisionHandler&&)		 = default;

	void Init() {
		/* Possibly add stuff here in the future. */
	}

	void Shutdown();
};

} // namespace ptgn