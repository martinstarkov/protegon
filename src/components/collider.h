#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

struct Collision {
	Collision() = default;

	Collision(ecs::Entity e1, ecs::Entity e2, const V2_float& normal) :
		entity1{ e1 }, entity2{ e2 }, normal{ normal } {}

	ecs::Entity entity1;
	ecs::Entity entity2;
	// Normal set to {} for overlap only collisions.
	V2_float normal;

	[[nodiscard]] bool operator==(const Collision& o) const {
		return entity1 == o.entity1 && entity2 == o.entity2 && normal == o.normal;
	}

	[[nodiscard]] bool operator!=(const Collision& o) const {
		return !(*this == o);
	}
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Collision> {
	std::size_t operator()(const ptgn::Collision& c) const noexcept {
		// Hashing combination algorithm from:
		// https://stackoverflow.com/a/17017281
		std::size_t hash{ 17 };
		hash = hash * 31 + std::hash<ecs::Entity>()(c.entity1);
		hash = hash * 31 + std::hash<ecs::Entity>()(c.entity2);
		hash = hash * 31 + std::hash<ptgn::V2_float>()(c.normal);
		return hash;
	}
};

namespace ptgn {

using CollisionCategory		 = std::int64_t;
using CollidesWithCategories = std::vector<CollisionCategory>;
using CollisionCallback		 = std::function<void(Collision)>;

enum class CollisionResponse {
	Slide,
	Bounce,
	Push
};

struct Collider {
	ecs::Entity parent;
	V2_float offset;
	Rect bounds;
	std::unordered_set<Collision> collisions;
	// Must return true for collisions to be checked.
	std::function<bool(ecs::Entity, ecs::Entity)> before_collision;
	CollisionCallback on_collision_start;
	CollisionCallback on_collision;
	CollisionCallback on_collision_stop;
	bool enabled{ true };
	// Overwrites continuous/regular collision in favor of overlap checks.
	bool overlap_only{ false };
	// Continuous collision detection for high velocity colliders.
	bool continuous{ false };
	// How the velocity of the sweep should respond to obstacles.
	// Not applicable if continuous is set to false.
	CollisionResponse response{ CollisionResponse::Slide };

	[[nodiscard]] CollisionCategory GetCollisionCategory() const;

	void SetCollisionCategory(const CollisionCategory& category);

	void ResetCollisionCategory();

	// Allow collider to collide with anything.
	void ResetCollidesWith();

	[[nodiscard]] bool ProcessCallback(ecs::Entity e1, ecs::Entity e2) const;

	[[nodiscard]] bool CanCollideWith(const Collider& c) const;

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const;

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const;

	void AddCollidesWith(const CollisionCategory& category);

	void RemoveCollidesWith(const CollisionCategory& category);

	void SetCollidesWith(const CollidesWithCategories& categories);

private:
	// Which categories this collider collides with.
	std::unordered_set<CollisionCategory> mask_;
	// Which category this collider is a part of.
	CollisionCategory category_{ 0 };
};

struct BoxCollider : public Collider {
	BoxCollider() = delete;

	BoxCollider(
		ecs::Entity parent, const V2_float& size = {}, Origin origin = Origin::Center,
		float rotation = 0.0f
	);

	// @return Rect in relative coordinates.
	[[nodiscard]] Rect GetRelativeRect() const;

	// @return Rect in absolute coordinates (relative to its parent entity's transform). If the
	// parent entity has a Animation or Sprite component, this will be relative to the top left of
	// that (plus the the transform as before).
	[[nodiscard]] Rect GetAbsoluteRect() const;

	V2_float size;
	Origin origin{ Origin::Center };
	// Rotation in radians relative to the center of the box collider, also relative to the parent
	// entity transform rotation.
	float rotation{ 0.0f };
};

struct CircleCollider : public Collider {
	float radius{ 0.0f };
};

// struct PolygonCollider : public Collider {
//	std::vector<V2_float> vertices;
// };

// TODO: Add edge collider.

struct ColliderGroup {
	using Name = std::string;

	ecs::Entity parent;

	ecs::Manager group;

	std::unordered_map<Name, ecs::Entity> names;

	explicit ColliderGroup(ecs::Entity parent, const ecs::Manager& group);

	// @param offset Relative position of the box collider.
	// @param rotation Relative rotation of the box collider.
	// @param size Relative size of the box collider.
	// @param origin Origin of the box collider relative to its local position.
	// @param enabled Enable/disable collider by default.
	ecs::Entity AddBox(
		const Name& name, const V2_float& position, float rotation, const V2_float& size,
		Origin origin = Origin::Center, bool enabled = true, CollisionCategory category = 0,
		const CollidesWithCategories& categories							  = {},
		const CollisionCallback& on_collision_start							  = nullptr,
		const CollisionCallback& on_collision								  = nullptr,
		const CollisionCallback& on_collision_stop							  = nullptr,
		const std::function<bool(ecs::Entity, ecs::Entity)>& before_collision = nullptr,
		bool overlap_only = false, bool continuous = false
	);

	[[nodiscard]] const BoxCollider& GetBox(const Name& name) const;

	// @return All child colliders (parent not included).
	[[nodiscard]] std::vector<ecs::Entity> GetAll() const;

	[[nodiscard]] ecs::Entity Get(const Name& name) const;
};

} // namespace ptgn