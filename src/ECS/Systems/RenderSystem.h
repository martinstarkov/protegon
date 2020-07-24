#pragma once

#include "System.h"

struct RenderComponent;
struct TransformComponent;

class RenderSystem : public System<RenderComponent, TransformComponent> {
public:
	virtual void update() override final;
};