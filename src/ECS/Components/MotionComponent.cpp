#include "MotionComponent.h"

#include "../EntityHandle.h"

#include "DragComponent.h"
#include "PlayerController.h"

#include <sstream>

#define TERMINAL_VELOCITY_PRECISION 2 // significant figures

double findTerminalVelocity(double x, double b, double c) {
	static double previous = x;
	double y = (x + c) * b;
	std::stringstream one, two;
	one << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << y;
	two << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << previous;
	if (one.str() != two.str()) {
		previous = y;
		return findTerminalVelocity(y, b, c);
	} else {
		return y;
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