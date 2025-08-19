#pragma once

#include "core/entity.h"

namespace ptgn {

// Owning version of an entity handle.
class GameObject : public Entity {
public:
	GameObject() = default;
	explicit GameObject(const Entity& entity);
	~GameObject();
	GameObject(GameObject&& other) noexcept;
	GameObject& operator=(GameObject&& other) noexcept;
	GameObject(const GameObject&)			 = delete;
	GameObject& operator=(const GameObject&) = delete;

	using Entity::Entity;
};

} // namespace ptgn