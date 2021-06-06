#pragma once

#include "core/ECS.h"
#include "components/TransformComponent.h"
#include "components/RigidBodyComponent.h"

namespace ptgn {

struct MovementSystem {
	void operator()(ecs::Entity entity,
					TransformComponent& transform,
					RigidBodyComponent& rigid_body) {
		rigid_body.body.velocity += rigid_body.body.acceleration;
		rigid_body.body.angular_velocity += rigid_body.body.angular_acceleration;
		transform.transform.position += rigid_body.body.velocity;
		transform.transform.rotation += rigid_body.body.angular_velocity;
	}
};

} // namespace ptgn