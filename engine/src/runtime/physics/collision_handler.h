#pragma once

#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/raycast.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/broadphase.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision.h"

namespace ptgn {

class Physics;
class Scene;
class SceneContext;

struct CollisionDebugSettings {
	/// @brief If true, draws continuous collision detection sweeps for debugging purposes.
	bool draw_ccd{ false };

	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	FillStyle draw_fill_style{ 1.0f };

	[[nodiscard]] bool DrawCCD() const {
		return draw_enabled && draw_ccd;
	}
};

class CollisionHandler {
public:
	[[nodiscard]] static bool CanCollide(
		Entity entity1, const Collider& collider1, Entity entity2, const Collider& collider2
	);

	void SetDebugSettings(const CollisionDebugSettings& settings = {});

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

	CollisionDebugSettings debug_settings_;

	constexpr static float slop_{ 0.0005f };
	constexpr static std::size_t max_sweep_iterations_{ 4 };
};

} // namespace ptgn