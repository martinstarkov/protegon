#pragma once

#include <utility>

#include "runtime/ecs/entity.h"

namespace ptgn {

/// Owning version of an entity handle.
class GameObject : public Entity {
public:
	GameObject() = default;

	explicit GameObject(Entity&& entity) : Entity{ std::move(entity) } {}

	~GameObject() noexcept {
		Entity::Destroy();
	}

	GameObject(GameObject&& other) noexcept :
		Entity{ std::exchange(static_cast<Entity&>(other), Entity{}) } {}

	GameObject& operator=(GameObject&& other) noexcept {
		if (this != &other) {
			Entity::Destroy();
			Entity::operator=(std::move(other));
		}
		return *this;
	}

	GameObject(const GameObject&)			 = delete;
	GameObject& operator=(const GameObject&) = delete;
};

} // namespace ptgn