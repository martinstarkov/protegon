#include "collision/collider.h"

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"
#include "utility/utility.h"

namespace ptgn {

bool Collider::CanCollideWith(const Collider& c) const {
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
	PTGN_ASSERT(parent != ecs::Entity{});
	std::vector<ecs::Entity> excluded{ parent };
	if (parent.Has<BoxColliderGroup>()) {
		excluded = ConcatenateVectors(excluded, parent.Get<BoxColliderGroup>().GetAll());
	}
	if (VectorContains(excluded, c.parent)) {
		return false;
	}
	return CanCollideWith(c.GetCollisionCategory());
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

bool Collider::ProcessCallback(ecs::Entity e1, ecs::Entity e2) const {
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
	for (const auto& category : categories) {
		AddCollidesWith(category);
	}
}

ecs::Entity Collider::GetParent(ecs::Entity owner) const {
	return parent == ecs::Entity{} ? owner : parent;
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

Rect Collider::GetAbsolute(Rect relative_rect) const {
	PTGN_ASSERT(parent.IsAlive());
	PTGN_ASSERT(parent.Has<Transform>());

	Transform transform{ parent.Get<Transform>() };

	// If parent has an animation, use coordinate relative to top left.
	if (parent.Has<Animation>()) {
		const Animation& anim = parent.Get<Animation>();
		Rect r{ anim.GetSource() };
		r.position		   = transform.position;
		transform.position = r.Min();
	} else if (parent.Has<Sprite>()) { // Prioritize animations over sprites.
		const Sprite& sprite = parent.Get<Sprite>();
		Rect source{ sprite.GetSource() };
		source.position	   = transform.position;
		transform.position = source.Min();
	}

	relative_rect.position += transform.position;
	relative_rect.rotation += transform.rotation;
	// Absolute value needed because scale can be negative for flipping.
	V2_float scale{ FastAbs(transform.scale.x), FastAbs(transform.scale.y) };
	relative_rect.position *= scale;
	relative_rect.size	   *= scale;
	return relative_rect;
}

BoxCollider::BoxCollider(ecs::Entity parent, const V2_float& size, Origin origin, float rotation) :
	size{ size }, origin{ origin }, rotation{ rotation } {
	this->parent = parent;
}

Rect BoxCollider::GetRelativeRect() const {
	return Rect{ offset, size, origin, rotation };
}

Rect BoxCollider::GetAbsoluteRect() const {
	return Collider::GetAbsolute(GetRelativeRect());
}

BoxColliderGroup::BoxColliderGroup(ecs::Entity parent, const ecs::Manager& group) :
	parent{ parent }, group{ group } {}

// @param offset Relative position of the box collider.
// @param rotation Relative rotation of the box collider.
// @param size Relative size of the box collider.
// @param origin Origin of the box collider relative to its local position.
// @param enabled Enable/disable collider by default.
ecs::Entity BoxColliderGroup::AddBox(
	const Name& name, const V2_float& position, float rotation, const V2_float& size, Origin origin,
	bool enabled, CollisionCategory category, const CollidesWithCategories& categories,
	const CollisionCallback& on_collision_start, const CollisionCallback& on_collision,
	const CollisionCallback& on_collision_stop,
	const std::function<bool(ecs::Entity, ecs::Entity)>& before_collision, bool overlap_only,
	bool continuous
) {
	auto entity{ group.CreateEntity() };
	auto& box{ entity.Add<BoxCollider>(parent, size, origin, rotation) };
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

const BoxCollider& BoxColliderGroup::GetBox(const Name& name) const {
	auto e = Get(name);
	PTGN_ASSERT(e.Has<BoxCollider>());
	return e.Get<BoxCollider>();
}

// @return All child colliders (parent not included).
std::vector<ecs::Entity> BoxColliderGroup::GetAll() const {
	return GetValues(names);
}

ecs::Entity BoxColliderGroup::Get(const Name& name) const {
	auto it{ names.find(name) };
	PTGN_ASSERT(it != names.end(), "Failed to retrieve entity with given name");
	return it->second;
}

CircleCollider::CircleCollider(ecs::Entity parent, float radius) : radius{ radius } {
	this->parent = parent;
}

Circle CircleCollider::GetRelativeCircle() const {
	return Circle{ offset, radius };
}

Circle CircleCollider::GetAbsoluteCircle() const {
	auto rect{ Collider::GetAbsolute(Rect{
		offset, { 2.0f * radius, 2.0f * radius }, Origin::Center, 0.0f }) };
	return Circle{ rect.Center(), radius };
}

} // namespace ptgn