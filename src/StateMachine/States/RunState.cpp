#include "RunState.h"
#include "StateCommon.h"

void RunState::onEntry() {
	SpriteComponent* sprite = parentEntity->getComponent<SpriteComponent>();
	if (sprite) {
		// TODO: Take state identifier from sprite sheet component
		sprite->source.y = sprite->source.h * 2;
	}
	AnimationComponent* animation = parentEntity->getComponent<AnimationComponent>();
	if (animation) {
		animation->counter = 0;
	}
}

void RunState::update() {
	MotionComponent* motion = parentEntity->getComponent<MotionComponent>();
	if (!(abs(motion->velocity) > IDLE_START_VELOCITY)) {
		parentStateMachine->setCurrentState("idle");
	} else if (!(abs(motion->velocity) >= motion->terminalVelocity * RUN_START_FRACTION)) {
		parentStateMachine->setCurrentState("walk");
	}
}
