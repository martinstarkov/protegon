#include "physics/collision/collider.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "math/geometry.h"
#include "physics/rigid_body.h"
#include "utility/span.h"

namespace ptgn {

bool PhysicsBody::IsImmovable() const {
	if (Has<RigidBody>() && Get<RigidBody>().immovable) {
		return true;
	}
	if (HasParent(*this)) {
		return PhysicsBody{ GetParent(*this) }.IsImmovable();
	}
	return false;
}

[[nodiscard]] Transform& PhysicsBody::GetRootTransform() {
	Entity root_entity{ GetRootEntity(*this) };
	PTGN_ASSERT(root_entity, "Physics body must have a valid root entity (or itself)");
	PTGN_ASSERT(root_entity.Has<Transform>(), "Root entity must have a transform component");
	return GetTransform(root_entity);
}

Collider::Collider(const Shape& shape) : shape{ shape } {}

Collider& Collider::SetOverlapMode() {
	mode = CollisionMode::Overlap;
	return *this;
}

Collider& Collider::SetCollisionMode(CollisionMode new_mode) {
	mode = new_mode;
	return *this;
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

bool Collider::CanCollideWith(const CollisionCategory& category) const {
	return mask_.empty() || VectorContains(mask_, category);
}

bool Collider::IsCategory(const CollisionCategory& category) const {
	return category_ == category;
}

void Collider::AddCollidesWith(const CollisionCategory& category) {
	PTGN_ASSERT(
		!VectorContains(mask_, category),
		"Cannot add the same collision category to a collider more than once"
	);
	mask_.emplace_back(category);
}

void Collider::RemoveCollidesWith(const CollisionCategory& category) {
	VectorErase(mask_, category);
}

void Collider::SetCollidesWith(const CollidesWithCategories& categories) {
	mask_.reserve(mask_.size() + categories.size());
	for (auto category : categories) {
		AddCollidesWith(category);
	}
}

Collision Collider::CollidedWith(const Entity& other) const {
	auto it{ std::ranges::find_if(collisions_, [&other](auto& collision) {
		return collision.entity == other;
	}) };
	return it != collisions_.end() ? *it : Collision{};
}

void Collider::ResetCollisions() {
	prev_collisions_ = collisions_;
	collisions_.clear();
}

void Collider::AddCollision(const Collision& collision) {
	if (VectorContains(collisions_, collision)) {
		return;
	}
	collisions_.emplace_back(collision);
}

} // namespace ptgn