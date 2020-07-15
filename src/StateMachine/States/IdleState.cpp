#include "IdleState.h"
#include "StateCommon.h"

void IdleState::update() {
	static int cycle = 0;
	if (cycle == 500) {
		getParentStateMachine().setCurrentState("walk");
	} else {
		cycle++;
	}
}
