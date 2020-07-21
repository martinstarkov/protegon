#include "JumpState.h"

#include "../../ECS/Components/MotionComponent.h"

void JumpState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (motion->acceleration.y >= 0.0) {
		parentStateMachine->setCurrentState("grounded");
	}
}
