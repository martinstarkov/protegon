#pragma once

#include <engine/Include.h>

class HopperPhysicsSystem : public ecs::System<TransformComponent, RigidBodyComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body] : entities) {
			auto& rb = rigid_body.rigid_body;
			// Gravity.
			rb.acceleration += rb.gravity;
			// Motion.
			rb.velocity += rb.acceleration;
			// Slow motion down
			rb.velocity *= 0.1;
		}
	}
};