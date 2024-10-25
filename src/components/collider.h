#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn {

using CollisionCategory		 = std::int64_t;
using CollidesWithCategories = std::vector<CollisionCategory>;
using CollisionCallback		 = std::function<void(ecs::Entity, ecs::Entity)>;

enum class CollisionResponse {
	Slide,
	Bounce,
	Push
};

struct Collider {
	ecs::Entity parent;
	V2_float offset;
	Rect bounds;
	std::unordered_set<ecs::Entity> collisions;
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

	[[nodiscard]] CollisionCategory GetCollisionCategory() const {
		return category_;
	}

	void SetCollisionCategory(const CollisionCategory& category) {
		category_ = category;
	}

	void ResetCollisionCategory() {
		category_ = 0;
	}

	// Allow collider to collide with anything.
	void ResetCollidesWith() {
		mask_ = {};
	}

	[[nodiscard]] bool CanCollideWith(const Collider& c) const {
		if (!enabled) {
			return false;
		}
		if (!c.enabled) {
			return false;
		}
		if (parent == c.parent) {
			return false;
		}
		if (!parent.IsAlive()) {
			return false;
		}
		if (!c.parent.IsAlive()) {
			return false;
		}
		return CanCollideWith(c.GetCollisionCategory());
	}

	[[nodiscard]] bool CanCollideWith(const CollisionCategory& category) const {
		return mask_.empty() || mask_.count(category) > 0;
	}

	[[nodiscard]] bool IsCategory(const CollisionCategory& category) const {
		return category_ == category;
	}

	void AddCollidesWith(const CollisionCategory& category) {
		mask_.insert(category);
	}

	void RemoveCollidesWith(const CollisionCategory& category) {
		mask_.erase(category);
	}

	void SetCollidesWith(const CollidesWithCategories& categories) {
		mask_.reserve(mask_.size() + categories.size());
		for (const auto& category : categories) {
			AddCollidesWith(category);
		}
	}

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
	) :
		size{ size }, origin{ origin }, rotation{ rotation } {
		this->parent = parent;
	}

	// @return Rect in relative coordinates.
	[[nodiscard]] Rect GetRelativeRect() const {
		return { offset, size, origin, rotation };
	}

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

	explicit ColliderGroup(ecs::Entity parent, const ecs::Manager& group) :
		parent{ parent }, group{ group } {}

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
	) {
		auto entity = group.CreateEntity();
		auto& box	= entity.Add<BoxCollider>(parent, size, origin, rotation);
		box.offset	= position;
		box.enabled = enabled;
		box.SetCollisionCategory(category);
		box.SetCollidesWith(categories);
		box.on_collision_start = on_collision_start;
		box.on_collision	   = on_collision;
		box.on_collision_stop  = on_collision_stop;
		box.before_collision   = before_collision;
		box.overlap_only	   = overlap_only;
		box.continuous		   = continuous;
		names.emplace(name, entity);
		group.Refresh();
		return entity;
	}

	const BoxCollider& GetBox(const Name& name) const {
		auto e = Get(name);
		PTGN_ASSERT(e.Has<BoxCollider>());
		return e.Get<BoxCollider>();
	}

	// @return All child colliders (parent not included).
	std::vector<ecs::Entity> GetAll() const {
		return GetValues(names);
	}

	ecs::Entity Get(const Name& name) const {
		auto it = names.find(name);
		PTGN_ASSERT(it != names.end(), "Failed to retrieve entity with given name");
		return it->second;
	}
};

} // namespace ptgn