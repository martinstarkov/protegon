#include "core/game_object.h"

#include <utility>

#include "core/entity.h"

namespace ptgn {

GameObject::GameObject(const Entity& entity) : Entity{ entity } {}

GameObject::~GameObject() {
	Destroy();
}

GameObject::GameObject(GameObject&& other) noexcept : Entity{ std::move(other) } {
	other.Invalidate();
}

GameObject& GameObject::operator=(GameObject&& other) noexcept {
	if (this != &other) {
		Destroy();
		Entity::operator=(std::move(other));
		other.Invalidate();
	}
	return *this;
}

} // namespace ptgn