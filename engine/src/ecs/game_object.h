#pragma once

#include <utility>

#include "core/util/concepts.h"
#include "ecs/entity.h"

namespace ptgn {

// TODO: Fix.
// Owning version of an entity handle.
// class GameObject {
// public:
//	GameObject() = default;
//
//	GameObject(const Entity& entity) : T{ entity } {}
//
//	GameObject(Entity&& entity) : T{ std::move(entity) } {}
//
//	GameObject(const Entity::BaseEntity& entity) : T{ entity } {}
//
//	~GameObject() {
//		Entity::Destroy();
//	}
//
//	GameObject(GameObject&& other) noexcept : T{ std::move(other) } {
//		other.Invalidate();
//	}
//
//	GameObject& operator=(GameObject&& other) noexcept {
//		if (this != &other) {
//			Entity::Destroy();
//			T::operator=(std::move(other));
//			other.Invalidate();
//		}
//		return *this;
//	}
//
//	GameObject(const GameObject&)			 = delete;
//	GameObject& operator=(const GameObject&) = delete;
//
//	using T::T;
//};

} // namespace ptgn