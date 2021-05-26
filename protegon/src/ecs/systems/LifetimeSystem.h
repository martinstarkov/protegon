#pragma once

#include "ecs/ECS.h"

#include "ecs/components/LifetimeComponent.h"

#include "core/Engine.h"

namespace engine {

class LifetimeSystem : public ecs::System<LifetimeComponent> {
public:
	virtual void Update() override final {
		for (auto& [entity, life] : entities) {
			if (life.countdown.Finished()) {
				entity.Destroy();
			}
		}
		GetManager().Refresh();
	}
};

} // namespace engine