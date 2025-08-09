#include "physics/collision/collider.h"

#include <functional>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
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
	auto root_entity{ GetRootEntity(*this) };
	PTGN_ASSERT(root_entity, "Physics body must have a valid root entity (or itself)");
	PTGN_ASSERT(root_entity.Has<Transform>(), "Root entity must have a transform component");
	return GetTransform(root_entity);
}

Collider::Collider(const Shape& shape) : shape{ shape } {}

Collider& Collider::SetOverlapOnly() {
	overlap_only = true;
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

bool Collider::ProcessCallback(Entity e1, Entity e2) const {
	// TODO: Fix script invocation.
	// if (!e1.Has<Scripts>()) {
	//	return true;
	//}
	// const auto& scripts{ e1.Get<Scripts>() };
	//// Check that each entity script allows for the pre-collision condition to pass.
	// for (const auto& [key, script] : scripts.scripts) {
	//	PTGN_ASSERT(script != nullptr, "Cannot invoke nullptr script");
	//	bool condition_met{ std::invoke(&impl::IScript::PreCollisionCondition, script, e2) };
	//	if (!condition_met) {
	//		return false;
	//	}
	// }

	return true;
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

void Collider::InvokeCollisionCallbacks(Entity& entity) const {
	// TODO: Make sure these are called without being able to be invalidated.
	// TODO: Fix script invocations.
	/*for (const auto& prev : prev_collisions_) {
		if (!VectorContains(collisions_, prev)) {
			InvokeScript<&impl::IScript::OnCollisionStop>(entity, prev);
		} else {
			InvokeScript<&impl::IScript::OnCollision>(entity, prev);
		}
	}
	for (const auto& current : collisions_) {
		if (!VectorContains(prev_collisions_, current)) {
			InvokeScript<&impl::IScript::OnCollisionStart>(entity, current);
		}
	}*/
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