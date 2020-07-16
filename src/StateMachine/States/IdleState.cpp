#include "IdleState.h"
#include "StateCommon.h"

void IdleState::onEntry() {
	SpriteComponent* sprite = parentEntity->getComponent<SpriteComponent>();
	if (sprite) {
		// TODO: Take state identifier from sprite sheet component
		sprite->source.y = sprite->source.h * 0;
	}
	AnimationComponent* animation = parentEntity->getComponent<AnimationComponent>();
	if (animation) {
		animation->counter = 0;
	}
}

void IdleState::update() {
	MotionComponent* motion = parentEntity->getComponent<MotionComponent>();
	if (abs(motion->velocity) >= IDLE_START_VELOCITY) {
		parentStateMachine->setCurrentState("walk");
	}
}