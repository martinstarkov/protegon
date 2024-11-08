#include "collision/collision.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "collision/collider.h"
#include "collision/raycast.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/intersection.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn::impl {

void CollisionHandler::Sweep(
	ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<Transform>& targets,
	bool debug_draw
) const {
	// TODO: Separate this code into functions.

	PTGN_ASSERT(game.dt() > 0.0f);

	if (!box.continuous || box.overlap_only || !entity.Has<RigidBody, Transform>()) {
		return;
	}

	const auto& transform{ entity.Get<Transform>() };
	auto& rigid_body{ entity.Get<RigidBody>() };

	const auto velocity = rigid_body.velocity * game.dt();

	if (velocity.IsZero()) {
		return;
	}

	Raycast c;

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
		// game.draw.Rect(rect.position, rect.size, color::Purple, rect.origin, 1.0f);
		if (e.Has<BoxCollider>()) {
			const auto& box2 = e.Get<BoxCollider>();
			if (box2.overlap_only || !box.CanCollideWith(box2)) {
				return false;
			}
			Rect rect2{ box2.GetAbsoluteRect() };
			// game.draw.Rect(rect2.position, rect2.size, color::Red, rect2.origin, 1.0f);
			dist2 = (rect.Center() - rect2.Center()).MagnitudeSquared();
			c	  = rect.Raycast(relative_velocity, rect2);
			return c.Occurred() && box.ProcessCallback(entity, e);
		} else if (e.Has<CircleCollider>()) {
			const auto& circle2 = e.Get<CircleCollider>();
			if (circle2.overlap_only || !box.CanCollideWith(circle2)) {
				return false;
			}
			Circle c2{ transform2.position + circle2.offset, circle2.radius };
			dist2 = (rect.Center() - c2.center).MagnitudeSquared();
			c	  = c2.Raycast(-relative_velocity, rect);
			return c.Occurred() && box.ProcessCallback(entity, e);
		}
		PTGN_ERROR("Unrecognized shape for collision check");
	};

	auto get_sorted_collisions = [&](const V2_float& offset, const V2_float& vel) {
		std::vector<SweepCollision> collisions;
		targets.ForEach([&](ecs::Entity e) {
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
			game.draw.Line(transform.position, new_p1, color::Gray);
		}
		return;
	}

	const auto& earliest{ collisions.front().c };

	const auto new_velocity = GetRemainingVelocity(velocity, earliest, box.response);

	const auto new_p1 = transform.position + velocity * earliest.t;

	if (debug_draw) {
		game.draw.Line(transform.position, new_p1, color::Blue);
		game.draw.Rect(new_p1, box.size, color::Purple, box.origin, 1.0f);
	}

	// Adds all collisions which occurred at the earliest time to box.collisions. This ensures all
	// callbacks are called.
	const auto add_earliest_collisions = [](ecs::Entity e,
											const std::vector<SweepCollision>& sweep_collisions,
											std::unordered_set<Collision>& entities) {
		PTGN_ASSERT(!sweep_collisions.empty());
		const auto& first_collision{ sweep_collisions.front() };
		PTGN_ASSERT(e != first_collision.e, "Self collision not possible");
		entities.emplace(e, first_collision.e, first_collision.c.normal);
		for (std::size_t i{ 1 }; i < sweep_collisions.size(); ++i) {
			const auto& collision{ sweep_collisions[i] };
			if (collision.c.t == first_collision.c.t) {
				PTGN_ASSERT(e != collision.e, "Self collision not possible");
				entities.emplace(e, collision.e, collision.c.normal);
			}
		}
	};

	add_earliest_collisions(entity, collisions, box.collisions);

	rigid_body.velocity *= earliest.t;

	if (new_velocity.IsZero()) {
		return;
	}

	const auto collisions2 = get_sorted_collisions(velocity * earliest.t, new_velocity);

	if (collisions2.empty()) {
		if (debug_draw) {
			game.draw.Line(new_p1, new_p1 + new_velocity, color::Orange);
		}
		rigid_body.velocity += new_velocity / game.dt();
		return;
	}

	const auto& earliest2{ collisions2.front().c };
	if (debug_draw) {
		game.draw.Line(new_p1, new_p1 + new_velocity * earliest2.t, color::Green);
	}
	add_earliest_collisions(entity, collisions2, box.collisions);
	rigid_body.velocity += new_velocity / game.dt() * earliest2.t;
}

void CollisionHandler::Overlap(
	ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets
) {
	if (!box.overlap_only) {
		return;
	}
	Rect r1{ box.GetAbsoluteRect() };
	for (auto [e2, b2] : targets) {
		if (!box.CanCollideWith(b2)) {
			continue;
		}
		Rect r2{ b2.GetAbsoluteRect() };
		if (r1.Overlaps(r2)) {
			ProcessCallback(box, entity, b2.GetParent(e2), {});
		}
	}
}

void CollisionHandler::Intersect(
	ecs::Entity entity, BoxCollider& box, const ecs::EntitiesWith<BoxCollider>& targets
) {
	if (box.overlap_only) {
		return;
	}
	// CONSIDER: Is this immovable criterion reasonable? Are there situations where we want an
	// intersection collision without a rigid body?
	if (!entity.Has<RigidBody>() || entity.Get<RigidBody>().immovable) {
		return;
	}

	auto& rb{ entity.Get<RigidBody>() };

	Rect r1{ box.GetAbsoluteRect() };

	Transform* transform{ nullptr };
	if (entity.Has<Transform>()) {
		transform = &entity.Get<Transform>();
	}

	for (auto [e2, b2] : targets) {
		if (b2.overlap_only || !box.CanCollideWith(b2)) {
			continue;
		}
		Rect r2{ b2.GetAbsoluteRect() };
		Intersection c{ r1.Intersects(r2) };
		if (!c.Occurred()) {
			continue;
		}
		ProcessCallback(box, entity, b2.GetParent(e2), c.normal);
		if (transform) {
			transform->position += c.normal * (c.depth);
			rb.velocity =
				GetRemainingVelocity(rb.velocity, Raycast{ 0.0f, c.normal }, box.response);
		}
	}
}

void CollisionHandler::SortCollisions(std::vector<SweepCollision>& collisions) {
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

V2_float CollisionHandler::GetRemainingVelocity(
	const V2_float& velocity, const Raycast& c, CollisionResponse response
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

void CollisionHandler::ProcessCallback(
	BoxCollider& b1, ecs::Entity e1_parent, ecs::Entity e2_parent, const V2_float& normal
) {
	if (b1.ProcessCallback(e1_parent, e2_parent)) {
		b1.collisions.emplace(e1_parent, e2_parent, normal);
	}
};

void CollisionHandler::Update(ecs::Manager& manager) const {
	auto box_colliders = manager.EntitiesWith<BoxCollider>();
	auto targets	   = manager.EntitiesWith<Transform>();

	// TODO: Add support for other collider types.

	for (auto [e1, b1] : box_colliders) {
		b1.ResetCollisions();

		ecs::Entity e{ b1.GetParent(e1) };

		Sweep(e, b1, targets);
		Overlap(e, b1, box_colliders);
		Intersect(e, b1, box_colliders);

		for (const auto& prev : b1.prev_collisions) {
			PTGN_ASSERT(e == prev.entity1);
			PTGN_ASSERT(e != prev.entity2);
		}
		for (const auto& current : b1.collisions) {
			PTGN_ASSERT(e == current.entity1);
			PTGN_ASSERT(e != current.entity2);
		}

		b1.InvokeCollisionCallbacks();
	}
}

} // namespace ptgn::impl