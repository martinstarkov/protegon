#pragma once

#include "core/ECS.h"
#include "components/HitboxComponent.h"
#include "components/TransformComponent.h"
#include "components/ShapeComponent.h"
#include "physics/collision/Collision.h"

namespace ptgn {

struct StaticCollisionSystem {
	void operator()(ecs::Entity entity, 
					HitboxComponent& hitbox, 
					TransformComponent& transform, 
					ShapeComponent& shape) {
		entity.GetManager().ForEachEntityWith<HitboxComponent, TransformComponent, ShapeComponent>(
			[&](ecs::Entity entity2,
				HitboxComponent& hitbox2,
				TransformComponent& transform2,
				ShapeComponent& shape2) {
			ResolveCollision(entity, entity2, transform, transform2, hitbox, hitbox2, shape, shape2);
		});
	}
};

} // namespace ptgn