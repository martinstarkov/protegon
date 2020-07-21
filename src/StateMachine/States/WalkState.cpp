#include "WalkState.h"

#include "../../ECS/Components/MotionComponent.h"
#include "../../ECS/Components/SpriteComponent.h"
#include "../../ECS/Components/AnimationComponent.h"

void WalkState::onEntry() {
	SpriteComponent* sprite = entity.getComponent<SpriteComponent>();
	if (sprite) {
		// TODO: Take state identifier from sprite sheet component
		sprite->source.y = sprite->source.h * 1;
	}
	AnimationComponent* animation = entity.getComponent<AnimationComponent>();
	if (animation) {
		animation->counter = 0;
	}
}

void WalkState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (abs(motion->velocity) >= motion->terminalVelocity * RUN_START_FRACTION) {
		parentStateMachine->setCurrentState("run");
	} else if (!(abs(motion->velocity) > IDLE_START_VELOCITY)) {
		parentStateMachine->setCurrentState("idle");
	}
}
