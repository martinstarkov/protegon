#include "math/collision.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "geometry/intersection.h"
#include "math.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"
#include "utility/utility.h"

namespace ptgn::impl {

bool DynamicCollisionHandler::LineLine(const Line& a, const Line& b, DynamicCollision& c) {
	// Source:
	// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect/565282#565282

	c = {};

	if (!a.Overlaps(b)) {
		return false;
	}

	V2_float r{ a.Direction() };
	V2_float s{ b.Direction() };

	float sr{ s.Cross(r) };
	if (NearlyEqual(sr, 0.0f)) {
		return false;
	}

	V2_float ab{ a.a - b.a };
	float abr{ ab.Cross(r) };

	if (float u{ abr / sr }; u < 0.0f || u > 1.0f) {
		return false;
	}

	V2_float ba{ b.a - a.a };
	float rs{ r.Cross(s) };
	if (NearlyEqual(rs, 0.0f)) {
		return false;
	}

	V2_float skewed{ -s.Skewed() };
	float mag2{ skewed.Dot(skewed) };
	if (NearlyEqual(mag2, 0.0f)) {
		return false;
	}

	float bas{ ba.Cross(s) };
	float t{ bas / rs };

	if (t < 0.0f || t > 1.0f) {
		return false;
	}

	c.t		 = t;
	c.normal = skewed / std::sqrt(mag2);
	return true;
}

bool DynamicCollisionHandler::LineCircle(const Line& a, const Circle& b, DynamicCollision& c) {
	// Source:
	// https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm/1084899#1084899

	c = {};

	if (!b.Overlaps(a)) {
		return false;
	}

	V2_float d{ -a.Direction() };
	V2_float f{ b.center - a.a };

	// bool (roots exist), float (root 1), float (root 2).
	auto [real, t1, t2] =
		QuadraticFormula(d.Dot(d), 2.0f * f.Dot(d), f.Dot(f) - b.radius * b.radius);

	if (!real) {
		return false;
	}

	bool w1{ t1 >= 0.0f && t1 <= 1.0f };
	bool w2{ t2 >= 0.0f && t2 <= 1.0f };

	// Pick the lowest collision time that is in the [0, 1] range.
	if (w1 && w2) {
		c.t = std::min(t1, t2);
	} else if (w1) {
		c.t = t1;
	} else if (w2) {
		c.t = t2;
	} else {
		return false;
	}

	V2_float impact{ b.center + d * c.t - a.a };

	float mag2{ impact.Dot(impact) };

	if (NearlyEqual(mag2, 0.0f)) {
		c = {};
		return false;
	}

	c.normal = -impact / std::sqrt(mag2);

	return true;
}

bool DynamicCollisionHandler::LineRect(const Line& a, Rect b, DynamicCollision& c) {
	c = {};

	bool start_in{ b.Overlaps(a.a) };
	bool end_in{ b.Overlaps(a.b) };

	if (start_in && end_in) {
		return false;
	}

	V2_float d{ a.Direction() };

	if (d.Dot(d) == 0.0f) {
		return false;
	}

	// TODO: Deal with situation where rectangle is inside the other rectangle.

	// Cache division.
	V2_float inv_dir{ 1.0f / d };

	// Calculate intersections with rectangle bounding axes.
	V2_float near{ b.Min() - a.a };
	V2_float far{ b.Max() - a.a };

	// Handle edge cases where the segment line is parallel with the edge of the rectangle.
	if (NearlyEqual(near.x, 0.0f)) {
		near.x = 0.0f;
	}
	if (NearlyEqual(near.y, 0.0f)) {
		near.y = 0.0f;
	}
	if (NearlyEqual(far.x, 0.0f)) {
		far.x = 0.0f;
	}
	if (NearlyEqual(far.y, 0.0f)) {
		far.y = 0.0f;
	}

	V2_float t_near{ near * inv_dir };
	V2_float t_far{ far * inv_dir };

	// Discard 0 / 0 divisions.
	if (std::isnan(t_far.y) || std::isnan(t_far.x)) {
		return false;
	}
	if (std::isnan(t_near.y) || std::isnan(t_near.x)) {
		return false;
	}

	// Sort axis collision times so t_near contains the shorter time.
	if (t_near.x > t_far.x) {
		std::swap(t_near.x, t_far.x);
	}
	if (t_near.y > t_far.y) {
		std::swap(t_near.y, t_far.y);
	}

	// Early rejection.
	if (t_near.x >= t_far.y || t_near.y >= t_far.x) {
		return false;
	}

	// Furthest time is contact on opposite side of target.
	// Reject if furthest time is negative, meaning the object is travelling away from the
	// target.
	float t_hit_far = std::min(t_far.x, t_far.y);
	if (t_hit_far < 0.0f) {
		return false;
	}

	if (NearlyEqual(t_near.x, t_near.y) && t_near.x == 1.0f) {
		return false;
	}

	// Closest time will be the first contact.
	bool interal{ start_in && !end_in };

	if (interal) {
		std::swap(t_near.x, t_far.x);
		std::swap(t_near.y, t_far.y);
		std::swap(inv_dir.x, inv_dir.y);
		c.t	 = std::min(t_near.x, t_near.y);
		d	*= -1.0f;
	} else {
		c.t = std::max(t_near.x, t_near.y);
	}

	if (c.t > 1.0f) {
		c = {};
		return false;
	}

	// Contact point of collision from parametric line equation.
	// c.point = a.a + c.time * d;

	// Find which axis collides further along the movement time.

	// TODO: Figure out how to fix biasing of one direction from one side and another on the
	// other side.
	bool equal_times{ NearlyEqual(t_near.x, t_near.y) };
	bool diagonal{ NearlyEqual(FastAbs(inv_dir.x), FastAbs(inv_dir.y)) };

	if (equal_times && diagonal) { // Both axes collide at the same time.
		// Diagonal collision, set normal to opposite of direction of movement.
		c.normal = { -Sign(d.x), -Sign(d.y) };
	}
	if (c.normal.IsZero()) {
		if (t_near.x > t_near.y) { // X-axis.
			// Direction of movement.
			if (inv_dir.x < 0.0f) {
				c.normal = { 1.0f, 0.0f };
			} else {
				c.normal = { -1.0f, 0.0f };
			}
		} else if (t_near.x < t_near.y) { // Y-axis.
			// Direction of movement.
			if (inv_dir.y < 0.0f) {
				c.normal = { 0.0f, 1.0f };
			} else {
				c.normal = { 0.0f, -1.0f };
			}
		}
	}

	if (interal) {
		std::swap(c.normal.x, c.normal.y);
		c.normal *= -1.0f;
	}

	// Raycast collision occurred.
	return true;
}

bool DynamicCollisionHandler::LineCapsule(const Line& a, const Capsule& b, DynamicCollision& c) {
	// Source: https://stackoverflow.com/a/52462458

	c = {};

	// TODO: Add early exit if overlap test fails.

	V2_float cv{ b.line.Direction() };
	float mag2{ cv.Dot(cv) };

	if (NearlyEqual(mag2, 0.0f)) {
		return LineCircle(a, { b.line.a, b.radius }, c);
	}

	float mag{ std::sqrt(mag2) };
	V2_float cu{ cv / mag };
	// Normal to b.line
	V2_float ncu{ cu.Skewed() };
	V2_float ncu_dist{ ncu * b.radius };

	Line p1{ b.line.a + ncu_dist, b.line.b + ncu_dist };
	Line p2{ b.line.a - ncu_dist, b.line.b - ncu_dist };

	DynamicCollision col_min{ c };
	if (LineLine(a, p1, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (LineLine(a, p2, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (LineCircle(a, { b.line.a, b.radius }, c) && c.t < col_min.t) {
		col_min = c;
	}
	if (LineCircle(a, { b.line.b, b.radius }, c) && c.t < col_min.t) {
		col_min = c;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		c = {};
		return false;
	}

	c = col_min;
	return true;
}

bool DynamicCollisionHandler::CircleCircle(
	const Circle& a, const V2_float& vel, const Circle& b, DynamicCollision& c
) {
	return LineCircle({ a.center, a.center + vel }, { b.center, b.radius + a.radius }, c);
}

bool DynamicCollisionHandler::CircleRect(
	const Circle& a, const V2_float& vel, const Rect& b, DynamicCollision& c
) {
	c = {};

	Line seg{ a.center, a.center + vel };

	bool start_inside{ a.Overlaps(b) };
	bool end_inside{ b.Overlaps({ seg.b, a.radius }) };

	if (start_inside && end_inside) {
		return false;
	}

	if (start_inside) {
		// Circle inside rectangle, flip segment direction.
		std::swap(seg.a, seg.b);
	}

	// Compute the rectangle resulting from expanding b by circle radius.
	Rect e;
	e.position = b.Min() - V2_float{ a.radius, a.radius };
	e.size	   = b.size + V2_float{ a.radius * 2.0f, a.radius * 2.0f };
	e.origin   = Origin::TopLeft;

	if (!seg.Overlaps(e)) {
		return false;
	}

	V2_float b_min{ b.Min() };
	V2_float b_max{ b.Max() };

	DynamicCollision col_min{ c };
	// Top segment.
	if (LineCapsule(seg, { { b_min, V2_float{ b_max.x, b_min.y } }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Right segment.
	if (LineCapsule(seg, { { V2_float{ b_max.x, b_min.y }, b_max }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Bottom segment.
	if (LineCapsule(seg, { { b_max, V2_float{ b_min.x, b_max.y } }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}
	// Left segment.
	if (LineCapsule(seg, { { V2_float{ b_min.x, b_max.y }, b_min }, a.radius }, c) &&
		c.t < col_min.t) {
		col_min = c;
	}

	if (NearlyEqual(col_min.t, 1.0f)) {
		return false;
	}

	if (start_inside) {
		col_min.t = 1.0f - col_min.t;
	}

	c = col_min;

	return true;
}

bool DynamicCollisionHandler::RectRect(
	const Rect& a, const V2_float& vel, const Rect& b, DynamicCollision& c
) {
	V2_float a_center{ a.Center() };
	bool occured = LineRect(
		{ a_center, a_center + vel }, { b.Min() - a.Half(), b.size + a.size, Origin::TopLeft }, c
	);
	bool collide_on_next_frame{ c.t < 1.0 && c.t >= 0.0f };
	return occured && collide_on_next_frame && !c.normal.IsZero();
}

V2_float DynamicCollisionHandler::Sweep(
	const std::vector<ecs::Entity>& excluded_entities, const RigidBody& rigid_body,
	const Transform& transform, BoxCollider& box, ecs::Manager& manager, CollisionResponse response,
	bool debug_draw
) {
	PTGN_ASSERT(game.dt() > 0.0f);

	const auto velocity = rigid_body.velocity * game.dt();

	if (velocity.IsZero()) {
		// TODO: Consider adding a static intersect check here.
		return rigid_body.velocity;
	}

	DynamicCollision c;

	auto targets = manager.EntitiesWith<Transform>();

	auto collision_occurred = [&](const V2_float& offset, const V2_float& vel, ecs::Entity e,
								  float& dist2) {
		if (!e.HasAny<BoxCollider, CircleCollider>()) {
			return false;
		}
		V2_float relative_velocity = vel;
		if (e.Has<RigidBody>()) {
			relative_velocity -= e.Get<RigidBody>().velocity * game.dt();
		}
		const auto& transform2 = e.Get<Transform>();
		Rect rect{ box.GetAbsoluteRect() };
		rect.position += offset;
		if (e.Has<BoxCollider>()) {
			const auto& box2 = e.Get<BoxCollider>();
			Rect rect2{ box2.GetAbsoluteRect() };
			dist2 = (rect.Center() - rect2.Center()).MagnitudeSquared();
			return RectRect(rect, relative_velocity, rect2, c);
		} else if (e.Has<CircleCollider>()) {
			const auto& circle2 = e.Get<CircleCollider>();
			Circle c2{ transform2.position + circle2.offset, circle2.radius };
			dist2 = (rect.Center() - c2.center).MagnitudeSquared();
			return CircleRect(c2, -relative_velocity, rect, c);
		}
		PTGN_ERROR("Unrecognized shape for collision check");
	};

	auto get_sorted_collisions = [&](const V2_float& offset, const V2_float& vel) {
		std::vector<SweepCollision> collisions;
		targets.ForEach([&](ecs::Entity e) {
			if (VectorContains(excluded_entities, e)) {
				return;
			}
			float dist2{ 0.0f };
			if (collision_occurred(offset, vel, e, dist2)) {
				collisions.emplace_back(c, dist2, e);
			}
		});
		SortCollisions(collisions);
		return collisions;
	};

	const auto collisions = get_sorted_collisions({}, velocity);

	if (collisions.empty()) { // no collisions occured.
		const auto new_p1 = transform.position + velocity;
		if (debug_draw) {
			game.draw.Line(transform.position, new_p1, color::Grey);
		}
		return rigid_body.velocity;
	}

	const auto& earliest{ collisions.front() };

	const auto new_velocity = GetRemainingVelocity(velocity, earliest.c, response);

	const auto new_p1 = transform.position + velocity * earliest.c.t;

	if (debug_draw) {
		game.draw.Line(transform.position, new_p1, color::Blue);
		game.draw.Rect(new_p1, box.size, color::Purple, box.origin, 1.0f);
	}

	box.collisions.insert(earliest.e);

	if (new_velocity.IsZero()) {
		return rigid_body.velocity * earliest.c.t;
	}

	if (const auto collisions2 = get_sorted_collisions(velocity * earliest.c.t, new_velocity);
		!collisions2.empty()) {
		const auto& earliest2{ collisions2[0] };
		if (debug_draw) {
			game.draw.Line(new_p1, new_p1 + new_velocity * earliest2.c.t, color::Green);
		}
		box.collisions.insert(earliest2.e);
		return rigid_body.velocity * earliest.c.t + new_velocity * earliest2.c.t / game.dt();
	}
	if (debug_draw) {
		game.draw.Line(new_p1, new_p1 + new_velocity, color::Orange);
	}
	return rigid_body.velocity * earliest.c.t + new_velocity / game.dt();
}

void DynamicCollisionHandler::SortCollisions(std::vector<SweepCollision>& collisions) {
	/*
	 * Initial sort based on distances of collision manifolds to the collider.
	 * This is required for RectVsRect collisions to prevent sticking
	 * to corners in certain configurations, such as if the player (o) gives
	 * a bottom right velocity into the following rectangle (x) configuration:
	 *       x
	 *     o x
	 *   x   x
	 * (player would stay still instead of moving down if this distance sort did not exist).
	 */
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) { return a.dist2 < b.dist2; }
	);
	// Sort based on collision times, and if they are equal, by collision normal magnitudes.
	std::sort(
		collisions.begin(), collisions.end(),
		[](const SweepCollision& a, const SweepCollision& b) {
			// If time of collision are equal, prioritize walls to corners, i.e. normals
			// (1,0) come before (1,1).
			if (a.c.t == b.c.t) {
				return a.c.normal.MagnitudeSquared() < b.c.normal.MagnitudeSquared();
			}
			// If collision times are not equal, sort by collision time.
			return a.c.t < b.c.t;
		}
	);
}

V2_float DynamicCollisionHandler::GetRemainingVelocity(
	const V2_float& velocity, const DynamicCollision& c, CollisionResponse response
) {
	float remaining_time{ 1.0f - c.t };

	switch (response) {
		case CollisionResponse::Slide: {
			V2_float tangent{ -c.normal.Skewed() };
			return velocity.Dot(tangent) * tangent * remaining_time;
		}
		case CollisionResponse::Push: {
			return Sign(velocity.Dot(-c.normal.Skewed())) * c.normal.Swapped() * remaining_time *
				   velocity.Magnitude();
		}
		case CollisionResponse::Bounce: {
			V2_float new_velocity = velocity * remaining_time;
			if (!NearlyEqual(FastAbs(c.normal.x), 0.0f)) {
				new_velocity.x *= -1.0f;
			}
			if (!NearlyEqual(FastAbs(c.normal.y), 0.0f)) {
				new_velocity.y *= -1.0f;
			}
			return new_velocity;
		}
		default: break;
	}
	PTGN_ERROR("Failed to identify DynamicCollisionResponse type");
}

void CollisionHandler::Shutdown() {
	dynamic = {};
}

void CollisionHandler::Update(ecs::Manager& manager) {
	PTGN_LOG("Updating box colliders");
	auto box_colliders = manager.EntitiesWith<BoxCollider>();
	for (auto [e1, b1] : box_colliders) {
		Rect r1{ b1.GetAbsoluteRect() };
		auto prev_collisions{ b1.collisions };
		b1.collisions.clear();
		ecs::Entity e{ b1.parent };
		bool child{ e != ecs::Entity{} };
		if (!child) {
			e = e1;
		}
		std::vector<ecs::Entity> excluded{ e };
		if (child && e.Has<ColliderGroup>()) {
			excluded = ConcatenateVectors(excluded, e.Get<ColliderGroup>().GetAll());
		}
		bool continuous{ !b1.overlap_only && b1.continuous && e.Has<RigidBody>() &&
						 e.Has<Transform>() };
		if (continuous) {
			V2_float sweep_vel = dynamic.Sweep(
				excluded, e.Get<RigidBody>(), e.Get<Transform>(), b1, manager, b1.response
			);
			e.Get<RigidBody>().velocity = sweep_vel;
			PTGN_LOG("Sweeping entity ", e.GetId(), ", final velocity: ", sweep_vel);
		}
		for (auto [e2, b2] : box_colliders) {
			if (!CanCollide(b1, b2) || VectorContains(excluded, e2)) {
				continue;
			}
			Rect r2{ b2.GetAbsoluteRect() };
			if (r1.Overlaps(r2)) {
				b1.collisions.insert(e);
			}
			if (!b1.overlap_only) {
				Intersection c{ r1.Intersects(r2) };
				if (c.Occurred()) {
					PTGN_LOG(
						"Entity ", e.GetId(), " intersected with entity ", e2.GetId(),
						", normal: ", c.normal, ", depth: ", c.depth
					);
					PTGN_ASSERT(e.Has<Transform>());
					e.Get<Transform>().position += c.normal * (c.depth + slop);
				}
			}
		}
		bool has_on_stop{ b1.on_collision_stop != nullptr };
		bool has_on_collision{ b1.on_collision != nullptr };
		if (has_on_collision || has_on_stop) {
			for (const auto& e_prev : prev_collisions) {
				PTGN_ASSERT(e != e_prev);
				if (b1.collisions.count(e_prev) == 0) {
					if (has_on_stop) {
						std::invoke(b1.on_collision_stop, e, e_prev);
					}
				} else if (has_on_collision) {
					std::invoke(b1.on_collision, e, e_prev);
				}
			}
		}
		if (b1.on_collision_start == nullptr) {
			continue;
		}
		for (const auto& e_prev : b1.collisions) {
			PTGN_ASSERT(e != e_prev);
			if (prev_collisions.count(e_prev) == 0) {
				std::invoke(b1.on_collision_start, e, e_prev);
			}
		}
	}
}

} // namespace ptgn::impl