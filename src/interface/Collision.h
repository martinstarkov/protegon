#pragma once

#include "math/Vector2.h"
#include "physics/Manifold.h"
#include "physics/shapes/AABB.h"
#include "core/ECS.h"
#include "components/Collider.h"
#include "components/Shape.h"
#include "components/Transform.h"
#include "components/RigidBody.h"

namespace ptgn {

namespace collision {

namespace internal {

using CollisionCallback = Manifold(*)(const component::Transform& A,
									  const component::Transform& B,
									  const component::Shape& a,
									  const component::Shape& b);

extern CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)];

Manifold StaticAABBvsAABB(const component::Transform& A,
						  const component::Transform& B,
						  const component::Shape& a,
						  const component::Shape& b);

Manifold StaticCirclevsCircle(const component::Transform& A,
							  const component::Transform& B,
							  const component::Shape& a,
							  const component::Shape& b);

Manifold StaticAABBvsCircle(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

Manifold StaticCirclevsAABB(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

} // namespace internal

Manifold StaticIntersection(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b);

void Clear(ecs::Manager& manager);

void Update(ecs::Manager& manager);

void Resolve(ecs::Manager& manager);

// Check if two AABBs overlap.
bool AABBvsAABB(const component::Transform& A,
				const component::Transform& B,
				const component::Shape& a,
				const component::Shape& b);

} // namespace collision

} // namespace ptgn