#pragma once

#include "System.h"

struct RenderComponent;
struct TransformComponent;
struct SizeComponent;

class RenderSystem : public System<RenderComponent, TransformComponent, SizeComponent> {
public:
	virtual void update() override final;
};