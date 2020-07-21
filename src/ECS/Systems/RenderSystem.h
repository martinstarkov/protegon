#pragma once

#include "System.h"

class RenderSystem : public System<RenderComponent, TransformComponent, SizeComponent> {
public:
	virtual void update() override final;
};