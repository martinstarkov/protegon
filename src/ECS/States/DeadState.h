#pragma once

#include "State.h"

class DeadState : public State<DeadState> {
public:
	virtual void enter(Entity& entity) override final {
		entity.removeComponents<AnimationComponent, MotionComponent>();
	}
	virtual void exit(Entity& entity) override final {}
};