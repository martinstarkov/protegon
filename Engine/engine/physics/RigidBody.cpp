#include "RigidBody.h"

#include <iomanip>
#include <sstream>

constexpr const int TERMINAL_VELOCITY_PRECISION = 2; // significant figures of precision before stopping terminal velocity recursive calculation

// function that uses recursion to see what velocity will converge to given certain drag and acceleration
static V2_double FindTerminalVelocity(const V2_double& drag, const V2_double& max_acceleration, V2_double initial_velocity = { 0.0, 0.0 }) {
	// store previous velocity for precision comparison to know when to exit recursion
	static auto previous_velocity = initial_velocity;
	auto velocity = (initial_velocity + max_acceleration) * drag;
	std::stringstream ss1, ss2;
	// limit the precision so the recursion doesn't take too long
	ss1 << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << velocity.x;
	ss2 << std::fixed << std::setprecision(TERMINAL_VELOCITY_PRECISION) << previous_velocity.x;
	if (ss1.str() != ss2.str()) {
		previous_velocity = velocity;
		return FindTerminalVelocity(drag, max_acceleration, velocity);
	} else {
		return velocity;
	}
}

V2_double RigidBody::GetMaximumAcceleration() const {
	return Abs(player_acceleration) + Abs(gravity);
}

void RigidBody::Init() {
	if (mass == 0.0) {
		inverse_mass = 0.0;
	} else {
		inverse_mass = 1.0 / mass;
	}
	ComputeTerminalVelocity();
}

void RigidBody::ComputeTerminalVelocity() {
	auto max_acceleration = GetMaximumAcceleration();
	if (terminal_velocity.IsInfinite() && !drag.IsZero() && !max_acceleration.IsZero() && !max_acceleration.IsInfinite()) { // terminal velocity not set
		terminal_velocity = FindTerminalVelocity(V2_double{ 1.0, 1.0 } - drag, max_acceleration);
	}
}