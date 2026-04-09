#pragma once

#include <ostream>
#include <vector>

#include "core/event/event.h"
#include "core/math/raycast.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/physics/broadphase.h"
#include "runtime/physics/collider.h"

namespace ptgn {

class Physics;
class Scene;
class SceneContext;

namespace event {

struct CollisionEvent : public Event<CollisionEvent> {
	CollisionEvent() = default;

	explicit CollisionEvent(const Collision& collision) : collision{ collision } {}

	operator Collision() const { // NOSONAR
		return collision;
	}

	Collision collision;
};

struct OverlapStart : public Event<OverlapStart> {
	OverlapStart() = default;

	explicit OverlapStart(Entity overlap_entity) : overlap_entity{ overlap_entity } {}

	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

struct OverlapContinue : public Event<OverlapContinue> {
	OverlapContinue() = default;

	explicit OverlapContinue(Entity overlap_entity) : overlap_entity{ overlap_entity } {}

	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

struct OverlapStop : public Event<OverlapStop> {
	OverlapStop() = default;

	explicit OverlapStop(Entity overlap_entity) : overlap_entity{ overlap_entity } {}

	operator Entity() const { // NOSONAR
		return overlap_entity;
	}

	Entity overlap_entity;
};

} // namespace event

struct CollisionHandlerSettings {
	/// @brief If true, draws continuous collision detection sweeps for debugging purposes.
	bool debug_draw_ccd{ false };

	bool debug_draw_enabled{ false };
	Color debug_draw_color{ color::Magenta };
	FillStyle debug_draw_fill_style{ FillStyle::Hollow(1.0f) };

	[[nodiscard]] bool DrawCCD() const {
		return debug_draw_enabled && debug_draw_ccd;
	}
};

namespace impl {

struct SweepCollision {
	SweepCollision() = default;

	SweepCollision(
		const RaycastResult& raycast_result, float distance_squared, Entity sweep_entity
	);

	/// @brief Collision entity.
	Entity entity;
	RaycastResult collision;
	float dist2{ 0.0f };

	friend std::ostream& operator<<(std::ostream& os, const SweepCollision& sweep_collision) {
		os << "{ entity: " << sweep_collision.entity;
		os << ", collision: " << sweep_collision.collision;
		os << ", dist2: " << sweep_collision.dist2 << " }";
		return os;
	}
};

} // namespace impl

class CollisionHandler {
public:
	[[nodiscard]] static bool CanCollide(
		Entity entity1, const Collider& collider1, Entity entity2, const Collider& collider2
	);

	void SetSettings(const CollisionHandlerSettings& settings = {});

private:
	friend class Physics;
	friend class Scene;
	friend class SceneContext;

	CollisionHandler()										 = default;
	~CollisionHandler() noexcept							 = default;
	CollisionHandler(CollisionHandler&&) noexcept			 = default;
	CollisionHandler& operator=(CollisionHandler&&) noexcept = default;
	CollisionHandler& operator=(const CollisionHandler&)	 = delete;
	CollisionHandler(const CollisionHandler&)				 = delete;

	void Overlap(Entity entity) const;

	void Intersect(Entity entity, secondsf dt);

	static std::vector<Entity> GetSweepCandidates(
		Entity entity1, V2_float velocity, const impl::KDTree& tree
	);

	/// @param offset Offset from the transform position of the entity. This enables doing a
	/// second sweep.
	/// @param vel Velocity of the entity. As above, this enables a second sweep in the direction
	/// of the remaining velocity.
	std::vector<impl::SweepCollision> GetSortedCollisions(
		Entity entity1, V2_float offset, V2_float velocity1, secondsf dt
	) const;

	/// @brief Adds all collisions which occurred at the earliest time to box.collisions. This
	/// ensures all callbacks are called.
	static void AddEarliestCollisions(
		Entity entity, const std::vector<impl::SweepCollision>& sweep_collisions
	);

	static void SortCollisions(std::vector<impl::SweepCollision>& collisions);

	static V2_float GetRemainingVelocity(
		V2_float velocity, const RaycastResult& collision, CollisionResponse response
	);

	static V2_float GetRelativeVelocity(V2_float velocity1, Entity entity2, secondsf dt);

	void UpdateKDTree(Entity entity, secondsf dt);

	/// @brief Updates the velocity of the object to prevent it from colliding with the target
	/// objects.
	void Sweep(Scene& scene, Entity entity, secondsf dt);

	/// @brief If debug draw enabled, draws the collider of the entity with the given position
	/// offset and color.
	void TryDrawDebugCollider(Scene& scene, Entity entity, V2_float offset, Color color) const;

	/// @brief If debug draw enabled, draws a line from the entity's position + start_offset to the
	/// entity's position + end_offset with the given color.
	void TryDrawDebugLine(
		Scene& scene, Entity entity, V2_float start_offset, V2_float end_offset, Color color
	) const;

	void Update(Scene& scene, secondsf dt);

	impl::KDTree static_tree_{ 100 };
	impl::KDTree dynamic_tree_{ 100 };

	CollisionHandlerSettings settings_;

	constexpr static float slop_{ 0.0005f };
	constexpr static std::size_t max_sweep_iterations_{ 4 };
};

} // namespace ptgn