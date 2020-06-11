#pragma once

#include "State.h"

class DeadState : public State<DeadState> {
public:
	// Countdown time in seconds
	DeadState(float countdown = 1.0f) : _countdown(static_cast<int>(countdown * FPS)) {}
	int _countdown;
	virtual void enter(Entity& entity) override final {
		entity.addSignature<DeadState>();
		SpriteComponent* sprite = entity.getComponent<SpriteComponent>();
		if (sprite) {
			sprite->_texture = TextureManager::load("./resources/textures/enemy.png");
		}
		entity.removeComponents<AnimationComponent, MotionComponent>();
	}
	virtual void exit(Entity& entity) override final {
		entity.removeSignature<DeadState>();
	}
};