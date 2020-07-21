#include "MotionComponent.h"

#include <sstream>

#include "../EntityHandle.h"

#include "DragComponent.h"
#include "PlayerController.h"

// IMPORTANT: Make sure to use maxSpeed for acceleration if calculating terminalVelocity when speed be higher than initially set

#define TERMINAL_VELOCITY_PRECISION 2 // significant figures

// function that uses recursion to see what velocity will converge to given certain drag and acceleration
static double findTerminalVelocity(double initialVelocity, double drag, double acceleration) {
	// store previous velocity for precision comparison to know when to exit recursion
	static double previous = initialVelocity;
	double velocity = (initialVelocity + acceleration) * drag;
	std::stringstream one, two;
	// limit the precision so the recursion doesn't take too long
	one << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << velocity;
	two << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << previous;
	if (one.str() != two.str()) {
		previous = velocity;
		return findTerminalVelocity(velocity, drag, acceleration);
	} else {
		return velocity;
	}
}

void MotionComponent::init() {
	DragComponent* drag = entity.getComponent<DragComponent>();
	PlayerController* controller = entity.getComponent<PlayerController>();
	if (drag && controller) {
		if (drag->drag.x == drag->drag.y && controller->speed.x == controller->speed.y) {
			double tV = findTerminalVelocity(0.0, 1.0 - drag->drag.x, controller->speed.x);
			terminalVelocity = Vec2D(tV, tV);
		} else {
			terminalVelocity = Vec2D(findTerminalVelocity(0.0, 1.0 - drag->drag.x, controller->speed.x), findTerminalVelocity(0.0, 1.0 - drag->drag.y, controller->speed.y));
		}
	}
}