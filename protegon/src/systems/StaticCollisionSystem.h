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
			if (entity != entity2 && hitbox.CanCollideWith(entity2) && hitbox2.CanCollideWith(entity)) {
				auto offset_transform{ transform };
				offset_transform.transform.position += hitbox.offset;
				auto offset_transform2{ transform2 };
				offset_transform2.transform.position += hitbox2.offset;
				auto manifold{ math::StaticCollisionCheck(offset_transform.transform,
														  offset_transform2.transform,
														  shape.shape,
														  shape2.shape) };
				if (manifold.CollisionOccured()) {
					hitbox.Resolve(entity2, manifold);
				}
			}
		});
	}
};

} // namespace ptgn