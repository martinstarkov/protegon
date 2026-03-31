#pragma once

#include <utility>

#include "runtime/ecs/entity.h"

namespace ptgn {

/// @brief Owning version of an entity handle.
template <EntityType T = Entity>
class GameObject : public T {
public:
	GameObject() = default;

	explicit GameObject(T&& entity) : T{ std::move(entity) } {}

	~GameObject() noexcept {
		Entity::Destroy();
	}

	GameObject(GameObject&& other) noexcept :
		T{ std::exchange(static_cast<Entity&>(other), Entity{}) } {}

	GameObject& operator=(GameObject&& other) noexcept {
		if (this != &other) {
			Entity::Destroy();
			T::operator=(std::move(other));
		}
		return *this;
	}

	GameObject(const GameObject&)			 = delete;
	GameObject& operator=(const GameObject&) = delete;

	using T::T;
};

} // namespace ptgn