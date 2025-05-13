#include "physics/collision/collider.h"

#include <functional>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "core/entity.h"
#include "physics/rigid_body.h"

namespace ptgn {

bool PhysicsBody::IsImmovable() const {
	if (Has<RigidBody>() && Get<RigidBody>().immovable) {
		return true;
	}
	if (HasParent()) {
		return PhysicsBody{ GetParent() }.IsImmovable();
	}
	return false;
}

[[nodiscard]] Transform& PhysicsBody::GetRootTransform() {
	auto root_entity{ Entity::GetRootEntity() };
	PTGN_ASSERT(root_entity, "Physics body must have a valid root entity (or itself)");
	PTGN_ASSERT(root_entity.Has<Transform>(), "Root entity must have a transform component");
	return root_entity.Get<Transform>();
}

CollisionCategory Collider::GetCollisionCategory() const {
	return category_;
}

void Collider::SetCollisionCategory(const CollisionCategory& category) {
	category_ = category;
}

void Collider::ResetCollisionCategory() {
	category_ = 0;
}

void Collider::ResetCollidesWith() {
	mask_ = {};
}

bool Collider::ProcessCallback(Entity e1, Entity e2) {
	return before_collision == nullptr || std::invoke(before_collision, e1, e2);
}

bool Collider::CanCollideWith(const CollisionCategory& category) const {
	return mask_.empty() || mask_.count(category) > 0;
}

bool Collider::IsCategory(const CollisionCategory& category) const {
	return category_ == category;
}

void Collider::AddCollidesWith(const CollisionCategory& category) {
	mask_.insert(category);
}

void Collider::RemoveCollidesWith(const CollisionCategory& category) {
	mask_.erase(category);
}

void Collider::SetCollidesWith(const CollidesWithCategories& categories) {
	mask_.reserve(mask_.size() + categories.size());
	for (auto category : categories) {
		AddCollidesWith(category);
	}
}

void Collider::InvokeCollisionCallbacks() {
	bool has_on_stop{ on_collision_stop != nullptr };
	bool has_on_collision{ on_collision != nullptr };
	if (has_on_collision || has_on_stop) {
		for (const auto& prev : prev_collisions) {
			if (collisions.count(prev) == 0) {
				if (has_on_stop) {
					std::invoke(on_collision_stop, prev);
				}
			} else if (has_on_collision) {
				std::invoke(on_collision, prev);
			}
		}
	}
	if (on_collision_start == nullptr) {
		return;
	}
	for (const auto& current : collisions) {
		if (prev_collisions.count(current) == 0) {
			std::invoke(on_collision_start, current);
		}
	}
}

void Collider::ResetCollisions() {
	prev_collisions = collisions;
	collisions.clear();
}

} // namespace ptgn