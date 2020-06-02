#pragma once
#include "Component.h"
#include "../Vec2D.h"

struct SizeComponent : public Component {
	static ComponentID ID;
	Vec2D _size;
	SizeComponent(EntityID id, Vec2D size = Vec2D()) : _size(size) {
		ID = createComponentID<SizeComponent>();
		_entityID = id;
	}
};

ComponentID SizeComponent::ID = 0;