#include "core/game_object.h"

#include "core/entity.h"
#include "core/manager.h"

namespace ptgn {

GameObject::GameObject(Manager& manager) : Entity{ manager.CreateEntity() } {}

GameObject::~GameObject() {
	Entity::Destroy();
}

Entity GameObject::GetEntity() const {
	return *this;
}

} // namespace ptgn