#pragma once

#include "core/ECS.h"
#include "components/LifetimeComponent.h"

namespace ptgn {

struct LifetimeSystem {
	void operator()(ecs::Entity entity, LifetimeComponent& lifetime) {
		if (lifetime.countdown.Finished()) {
			entity.Destroy();
		}
	}
};


} // namespace ptgn