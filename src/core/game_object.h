#pragma once

#include <utility>

#include "common/concepts.h"
#include "core/entity.h"

namespace ptgn {

// Owning version of an entity handle.
template <typename T = Entity>
	requires IsOrDerivedFrom<T, Entity>
class GameObject : public T {
public:
	GameObject() = default;

	GameObject(const Entity& entity) : T{ entity } {}

	GameObject(Entity&& entity) : T{ std::move(entity) } {}

	GameObject(const Entity::EntityBase& entity) : T{ entity } {}

	~GameObject() {
		T::Destroy();
	}

	GameObject(GameObject&& other) noexcept : T{ std::move(other) } {
		other.Invalidate();
	}

	GameObject& operator=(GameObject&& other) noexcept {
		if (this != &other) {
			T::Destroy();
			T::operator=(std::move(other));
			other.Invalidate();
		}
		return *this;
	}

	GameObject(const GameObject&)			 = delete;
	GameObject& operator=(const GameObject&) = delete;

	using T::T;
};

} // namespace ptgn