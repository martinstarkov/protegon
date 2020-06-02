#pragma once
#include "Component.h"
#include "./Vec2D.h"

struct RenderComponent : public Component {
	static ComponentID ID;
	RenderComponent() {
		ID = createComponentID<RenderComponent>();
	}
};

ComponentID RenderComponent::ID = 0;