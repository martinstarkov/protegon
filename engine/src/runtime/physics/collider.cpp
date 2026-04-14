#include "runtime/physics/collider.h"

#include <algorithm>
#include <ostream>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/shape.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

Collider::Collider(const ColliderShape& shape) : shape{ shape } {}

Collider& Collider::SetOverlapMode() {
	mode = CollisionMode::Overlap;
	return *this;
}

Collider& Collider::SetCollisionMode(CollisionMode new_mode) {
	mode = new_mode;
	return *this;
}

ColliderMask Collider::GetMask() const {
	return mask_;
}

Collider& Collider::SetMask(ColliderMask mask) {
	mask_ = mask;
	return *this;
}

Collider& Collider::ResetMask() {
	mask_ = 0;
	return *this;
}

Collider& Collider::ResetCollidesWith() {
	collides_with_masks_ = {};
	return *this;
}

bool Collider::CanCollideWith(ColliderMask mask) const {
	return collides_with_masks_.empty() || std::ranges::contains(collides_with_masks_, mask);
}

bool Collider::IsMask(ColliderMask mask) const {
	return mask_ == mask;
}

Collider& Collider::AddCollidesWith(ColliderMask mask) {
	PTGN_ASSERT(
		!std::ranges::contains(collides_with_masks_, mask),
		"Cannot add the same collision mask to a collider more than once"
	);
	collides_with_masks_.emplace_back(mask);
	return *this;
}

Collider& Collider::RemoveCollidesWith(ColliderMask mask) {
	std::erase(collides_with_masks_, mask);
	return *this;
}

Collider& Collider::SetCollidesWith(const std::vector<ColliderMask>& masks) {
	collides_with_masks_.reserve(collides_with_masks_.size() + masks.size());
	for (auto mask : masks) {
		AddCollidesWith(mask);
	}
	return *this;
}

static CollisionInfo GetIfExists(const std::vector<CollisionInfo>& collisions, Entity other) {
	auto it{ std::ranges::find_if(collisions, [&other](auto& collision) {
		return collision.entity == other;
	}) };
	return it != collisions.end() ? *it : CollisionInfo{};
}

CollisionInfo Collider::IntersectedWith(Entity other) const {
	return GetIfExists(intersects_, other);
}

CollisionInfo Collider::SweptWith(Entity other) const {
	return GetIfExists(sweeps_, other);
}

bool Collider::OverlappedWith(Entity other) const {
	return std::ranges::contains(overlaps_, other);
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

void Collider::AddOverlap(Entity other) {
	if (OverlappedWith(other)) {
		return;
	}
	overlaps_.emplace_back(other);
}

void Collider::AddIntersect(const CollisionInfo& collision) {
	if (std::ranges::contains(intersects_, collision)) {
		return;
	}
	intersects_.emplace_back(collision);
}

void Collider::AddSweep(const CollisionInfo& collision) {
	if (std::ranges::contains(sweeps_, collision)) {
		return;
	}
	sweeps_.emplace_back(collision);
}

std::ostream& operator<<(std::ostream& os, CollisionResponse response) {
	switch (response) {
		using enum CollisionResponse;
		case Slide:	 return os << "Slide";
		case Bounce: return os << "Bounce";
		case Push:	 return os << "Push";
		case Stick:	 return os << "Stick";
		default:	 PTGN_ERROR("Unknown CollisionResponse: ", std::to_underlying(response));
	}
}

std::ostream& operator<<(std::ostream& os, CollisionMode mode) {
	switch (mode) {
		using enum CollisionMode;
		case None:		 return os << "None";
		case Overlap:	 return os << "Overlap";
		case Discrete:	 return os << "Discrete";
		case Continuous: return os << "Continuous";
		default:		 PTGN_ERROR("Unknown CollisionMode: ", std::to_underlying(mode));
	}
}

} // namespace ptgn