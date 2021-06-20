#pragma once

#include "core/ECS.h"
#include "components/TransformComponent.h"
#include "components/HitboxComponent.h"
#include "components/ShapeComponent.h"
#include "physics/Transform.h"
#include "physics/Manifold.h"
#include "physics/shapes/Shape.h"

namespace ptgn {

namespace math {

using CollisionCallback = Manifold (*)(const Transform& A, 
									   const Transform& B, 
									   Shape* const a, 
									   Shape* const b);

extern CollisionCallback StaticCollisionDispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)];

inline Manifold StaticCollisionCheck(const Transform& A, 
									 const Transform& B, 
									 Shape* const a, 
									 Shape* const b) {
	return StaticCollisionDispatch[static_cast<int>(a->GetType())][static_cast<int>(b->GetType())](A, B, a, b);
}

} // namespace math

inline void ResolveCollision(ecs::Entity& entity,
							 ecs::Entity& entity2,
							 TransformComponent& transform,
							 TransformComponent& transform2,
							 HitboxComponent& hitbox,
							 HitboxComponent& hitbox2,
							 ShapeComponent& shape,
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
			hitbox.Resolve(entity, entity2, manifold);
		}
	}
}

} // namespace ptgn