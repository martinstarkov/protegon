#pragma once

#include "math/Vector2.h"

namespace engine {

struct RigidBody {
	RigidBody() = default;
	RigidBody(const V2_double& velocity) : velocity{ velocity } {}
	RigidBody(const V2_double& velocity, const V2_double& acceleration) : 
		velocity{ velocity }, 
		acceleration{ acceleration }
	{}
	RigidBody(const V2_double& velocity, const V2_double& acceleration, double angular_velocity, double angular_acceleration) :
		velocity{ velocity },
		acceleration{ acceleration },
		angular_velocity{ angular_velocity },
		angular_acceleration{ angular_acceleration }
	{}
	V2_double velocity;
	V2_double acceleration;
	double angular_velocity{ 0 };
	double angular_acceleration{ 0 };
};

} // namespace engine