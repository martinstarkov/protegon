#include "math/collision.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "geometry/intersection.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/raycast.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn::impl {

V2_float DynamicCollisionHandler::Sweep(
	ecs::Entity entity, const RigidBody& rigid_body, const Transform& transform, BoxCollider& box,
	ecs::Manager& manager, CollisionResponse response, bool debug_draw
) {
	PTGN_ASSERT(game.dt() > 0.0f);

	const auto velocity = rigid_body.velocity * game.dt();

	if (velocity.IsZero()) {
		// TODO: Consider adding a static intersect check here.
		return rigid_body.velocity;
	}

	Raycast c;

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
		// game.draw.Rect(rect.position, rect.size, color::Purple, rect.origin, 1.0f);
		if (e.Has<BoxCollider>()) {
			const auto& box2 = e.Get<BoxCollider>();
			if (!box.CanCollideWith(box2)) {
				return false;
			}
			Rect rect2{ box2.GetAbsoluteRect() };
			// game.draw.Rect(rect2.position, rect2.size, color::Red, rect2.origin, 1.0f);
			dist2 = (rect.Center() - rect2.Center()).MagnitudeSquared();
			c	  = rect.Raycast(relative_velocity, rect2);
			return c.Occurred() && box.ProcessCallback(entity, e);
		} else if (e.Has<CircleCollider>()) {
			const auto& circle2 = e.Get<CircleCollider>();
			if (!box.CanCollideWith(circle2)) {
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
		return rigid_body.velocity;
	}

	const auto& earliest{ collisions.front().c };

	const auto new_velocity = GetRemainingVelocity(velocity, earliest, response);

	const auto new_p1 = transform.position + velocity * earliest.t;

	if (debug_draw) {
		game.draw.Line(transform.position, new_p1, color::Blue);
		game.draw.Rect(new_p1, box.size, color::Purple, box.origin, 1.0f);
	}

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

	if (new_velocity.IsZero()) {
		return rigid_body.velocity * earliest.t;
	}

	if (const auto collisions2 = get_sorted_collisions(velocity * earliest.t, new_velocity);
		!collisions2.empty()) {
		const auto& earliest2{ collisions2.front().c };
		if (debug_draw) {
			game.draw.Line(new_p1, new_p1 + new_velocity * earliest2.t, color::Green);
		}
		add_earliest_collisions(entity, collisions2, box.collisions);
		return rigid_body.velocity * earliest.t + new_velocity * earliest2.t / game.dt();
	}
	if (debug_draw) {
		game.draw.Line(new_p1, new_p1 + new_velocity, color::Orange);
	}
	return rigid_body.velocity * earliest.t + new_velocity / game.dt();
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

void CollisionHandler::Shutdown() {
	dynamic = {};
}

void CollisionHandler::Update(ecs::Manager& manager) {
	// TODO: Clean up / Refactor this function.
	auto box_colliders = manager.EntitiesWith<BoxCollider>();
	for (auto [e1, b1] : box_colliders) {
		Rect r1{ b1.GetAbsoluteRect() };
		auto prev_collisions{ b1.collisions };
		b1.collisions.clear();
		ecs::Entity e{ b1.parent == ecs::Entity{} ? e1 : b1.parent };
		if (b1.continuous && !b1.overlap_only && e.Has<RigidBody>() && e.Has<Transform>()) {
			V2_float sweep_vel =
				dynamic.Sweep(e, e.Get<RigidBody>(), e.Get<Transform>(), b1, manager, b1.response);
			e.Get<RigidBody>().velocity = sweep_vel;
		}
		for (auto [e2, b2] : box_colliders) {
			if (!b1.CanCollideWith(b2)) {
				continue;
			}
			Rect r2{ b2.GetAbsoluteRect() };
			if (b1.overlap_only && r1.Overlaps(r2)) {
				ecs::Entity ep{ e2 };
				if (b2.parent != ecs::Entity{}) {
					ep = b2.parent;
				}
				if (b1.ProcessCallback(e, ep)) {
					b1.collisions.emplace(e, ep, V2_float{});
				}
			}
			// TODO: Check that this immovable is reasonable. Are there situations where we want an
			// intersection collision without a rigid body?
			if (!b1.overlap_only && e.Has<RigidBody>() && !e.Get<RigidBody>().immovable) {
				Intersection c{ r1.Intersects(r2) };
				if (c.Occurred()) {
					ecs::Entity ep{ b2.parent == ecs::Entity{} ? e2 : b2.parent };
					if (b1.ProcessCallback(e, ep)) {
						b1.collisions.emplace(e, ep, c.normal);
					}
					PTGN_ASSERT(e.Has<Transform>());
					e.Get<Transform>().position += c.normal * (c.depth + slop);
				}
			}
		}
		bool has_on_stop{ b1.on_collision_stop != nullptr };
		bool has_on_collision{ b1.on_collision != nullptr };
		if (has_on_collision || has_on_stop) {
			for (const auto& prev : prev_collisions) {
				PTGN_ASSERT(e == prev.entity1);
				PTGN_ASSERT(e != prev.entity2);
				if (b1.collisions.count(prev) == 0) {
					if (has_on_stop) {
						std::invoke(b1.on_collision_stop, prev);
					}
				} else if (has_on_collision) {
					std::invoke(b1.on_collision, prev);
				}
			}
		}
		if (b1.on_collision_start == nullptr) {
			continue;
		}
		for (const auto& prev : b1.collisions) {
			PTGN_ASSERT(e == prev.entity1);
			PTGN_ASSERT(e != prev.entity2);
			if (prev_collisions.count(prev) == 0) {
				std::invoke(b1.on_collision_start, prev);
			}
		}
	}
}

} // namespace ptgn::impl