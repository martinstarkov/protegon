#include "GroundedState.h"

#include "../../ECS/Components/MotionComponent.h"

void GroundedState::update() {
	MotionComponent* motion = entity.getComponent<MotionComponent>();
	if (motion->acceleration.y < 0.0) {
		parentStateMachine->setCurrentState("jumped");
	}
}
