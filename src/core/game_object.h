#pragma once

#include "core/entity.h"
#include "core/manager.h"

namespace ptgn {

class GameObject : public Entity {
public:
	GameObject() = default;
	explicit GameObject(Manager& manager);
	GameObject(const GameObject&)				 = delete;
	GameObject& operator=(const GameObject&)	 = delete;
	GameObject(GameObject&&) noexcept			 = default;
	GameObject& operator=(GameObject&&) noexcept = default;
	virtual ~GameObject();

	operator Entity() const;

	Entity GetEntity() const;
};

} // namespace ptgn