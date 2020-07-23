#include "IdleState.h"

#include "StateCommon.h"

void IdleState::onEntry() {
	SpriteComponent* sprite = entity.getComponent<SpriteComponent>();
	if (sprite) {
		// TODO: Take state identifier from sprite sheet component
		sprite->source.y = sprite->source.h * 0;
	}
	AnimationComponent* animation = entity.getComponent<AnimationComponent>();
	if (animation) {
		animation->counter = 0;
	}
}

void IdleState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (abs(motion->velocity) >= IDLE_START_VELOCITY) {
		parentStateMachine->setCurrentState("walk");
	}
}