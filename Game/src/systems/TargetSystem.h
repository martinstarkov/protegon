#pragma once

#include <engine/Include.h>

#include "components/TargetComponent.h"

class TargetSystem : public ecs::System<TransformComponent, RigidBodyComponent, TargetComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, transform, rigid_body, target] : entities) {
			auto& rb = rigid_body.rigid_body;
			//DebugDisplay::lines().emplace_back(target.target_position, transform.position, engine::RED);
			//rb.velocity = (target.target_position - transform.position).Normalized() * target.approach_speed;
		}
	}
};