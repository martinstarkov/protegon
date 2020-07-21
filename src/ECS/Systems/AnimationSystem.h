#pragma once

#include "System.h"

struct AnimationComponent;
struct SpriteComponent;

// TODO: Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public System<AnimationComponent, SpriteComponent> {
public:
	virtual void update() override final;
};