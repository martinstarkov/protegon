#include "math/collision/collider.h"

#include <functional>
#include <type_traits>
#include <vector>

#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"

namespace ptgn {

bool Collision::operator==(const Collision& o) const {
	return entity1 == o.entity1 && entity2 == o.entity2 && normal == o.normal;
}

bool Collision::operator!=(const Collision& o) const {
	return !(*this == o);
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

bool Collider::ProcessCallback(ecs::Entity e1, ecs::Entity e2) {
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

BoxCollider::BoxCollider(const ecs::Entity& e) : Rect{ e } {}

BoxCollider::BoxCollider(const ecs::Entity& e, const V2_float& size, Origin origin) :
	Rect{ e, size, origin } {}

CircleCollider::CircleCollider(const ecs::Entity& e) : Circle{ e } {}

CircleCollider::CircleCollider(const ecs::Entity& e, float radius) : Circle{ e, radius } {}

} // namespace ptgn