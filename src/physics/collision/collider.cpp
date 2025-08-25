#include "physics/collision/collider.h"

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "utility/span.h"

namespace ptgn {

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

[[nodiscard]] static Collision GetIfExists(
	const std::vector<Collision>& collisions, const Entity& other
) {
	auto it{ std::ranges::find_if(collisions, [&other](auto& collision) {
		return collision.entity == other;
	}) };
	return it != collisions.end() ? *it : Collision{};
}

Collision Collider::IntersectedWith(const Entity& other) const {
	return GetIfExists(intersects_, other);
}

Collision Collider::SweptWith(const Entity& other) const {
	return GetIfExists(sweeps_, other);
}

bool Collider::OverlappedWith(const Entity& other) const {
	return VectorContains(overlaps_, other);
}

void Collider::ResetContainers() {
	ResetOverlaps();
	ResetIntersects();
	ResetSweeps();
}

void Collider::ResetOverlaps() {
	previous_overlaps_ = overlaps_;
	overlaps_.clear();
}

void Collider::ResetIntersects() {
	previous_intersects_ = intersects_;
	intersects_.clear();
}

void Collider::ResetSweeps() {
	previous_sweeps_ = sweeps_;
	sweeps_.clear();
}

void Collider::AddOverlap(const Entity& other) {
	if (OverlappedWith(other)) {
		return;
	}
	overlaps_.emplace_back(other);
}

void Collider::AddIntersect(const Collision& collision) {
	if (VectorContains(intersects_, collision)) {
		return;
	}
	intersects_.emplace_back(collision);
}

void Collider::AddSweep(const Collision& collision) {
	if (VectorContains(sweeps_, collision)) {
		return;
	}
	sweeps_.emplace_back(collision);
}

} // namespace ptgn