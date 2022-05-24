#include "Collision.h"

namespace ptgn {

namespace collision {

namespace internal {

CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::internal::physics::ShapeType::COUNT)] = {
    { StaticCirclevsCircle, StaticCirclevsAABB },
    { StaticAABBvsCircle, StaticAABBvsAABB }
};

Manifold StaticAABBvsAABB(const component::Transform& A,
						  const component::Transform& B,
						  const component::Shape& a,
						  const component::Shape& b) {
	assert(a.instance != nullptr && "Cannot generate manifold for non-existent shape");
	assert(b.instance != nullptr && "Cannot generate manifold for non-existent shape");

	physics::AABB aabb{ a.instance->CastTo<physics::AABB>() };
	physics::AABB aabb2{ b.instance->CastTo<physics::AABB>() };

	// Use center positions.
	const auto half_1{ aabb.size / 2.0 };
	const auto half_2{ aabb2.size / 2.0 };

	auto top_left_1{ A.position };
	auto top_left_2{ B.position };

	top_left_1 += half_1;
	top_left_2 += half_2;

	const auto dx{ top_left_2.x - top_left_1.x };
	const auto px{ (half_2.x + half_1.x) - math::Abs(dx) };

	Manifold manifold;

	if (px <= 0) {
		return manifold;
	}
	const auto dy{ top_left_2.y - top_left_1.y };
	const auto py{ (half_2.y + half_1.y) - math::Abs(dy) };
	if (py <= 0) {
		return manifold;
	}
	if (px < py) {
		const auto sx{ math::Sign(dx) };
		manifold.penetration.x = px * sx;
		manifold.normal.x = sx;
		manifold.contact_point.x = top_left_1.x + (half_1.x * sx);
		manifold.contact_point.y = top_left_2.y;
	} else {
		const auto sy{ math::Sign(dy) };
		manifold.penetration.y = py * sy;
		manifold.normal.y = sy;
		manifold.contact_point.x = top_left_2.x;
		manifold.contact_point.y = top_left_1.y + (half_1.y * sy);
	}
	return manifold;
}

// TODO: Fill.
Manifold StaticCirclevsCircle(const component::Transform& A, const component::Transform& B, const component::Shape& a, const component::Shape& b) {
	return {};
}

// TODO: Fill.
Manifold StaticAABBvsCircle(const component::Transform& A, const component::Transform& B, const component::Shape& a, const component::Shape& b) {
	return {};
}

Manifold StaticCirclevsAABB(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b) {
	Manifold manifold{ StaticAABBvsCircle(B, A, b, a) };
	manifold.normal *= -1;
	manifold.penetration *= -1;
	return manifold;
}

} // namespace internal

Manifold StaticIntersection(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b) {
	return internal::StaticCollisionDispatch[static_cast<int>(a.instance->GetType())][static_cast<int>(b.instance->GetType())](A, B, a, b);
}

void Clear(ecs::Manager& manager) {
	manager.ForEachEntityWith<component::Collider>([](ecs::Entity& entity, component::Collider& collider) {
		collider.Clear();
	});
}

void Update(ecs::Manager& manager) {
	manager.ForEachEntityWith<component::Collider, component::Transform, component::Shape>([&](ecs::Entity& entityA, component::Collider& colliderA, component::Transform& transformA, component::Shape& shapeA) {
		manager.ForEachEntityWith<component::Collider, component::Transform, component::Shape>([&](ecs::Entity& entityB, component::Collider& colliderB, component::Transform& transformB, component::Shape& shapeB) {
			if (colliderA.collideable && colliderB.collideable && entityA != entityB) {
				auto manifold = collision::StaticIntersection(transformA, transformB, shapeA, shapeB);
				if (manifold.CollisionOccured()) {
					colliderA.manifolds.emplace_back(manifold);
					colliderB.manifolds.emplace_back(manifold);
					// TODO: Move this out, into some resolve function with templates possibly? Component for resolving collisions?
					if (entityA.HasComponent<component::RigidBody>()) {
						auto& rigid_bodyA = entityA.GetComponent<component::RigidBody>();
						auto normal_flip = manifold.normal.Flip();
						rigid_bodyA.velocity = rigid_bodyA.velocity.DotProduct(normal_flip) * normal_flip;
						transformA.position -= manifold.penetration;
					};
				}
			}
		});
	});
}

void Resolve(ecs::Manager& manager) {
	// TODO: Figure out how to resolve different types of collisions. Lambdas with template args? Component approach?
}

bool AABBvsAABB(const component::Transform& A,
				const component::Transform& B,
				const component::Shape& a,
				const component::Shape& b) {
	assert(a.instance != nullptr && "Cannot generate manifold for non-existent shape");
	assert(b.instance != nullptr && "Cannot generate manifold for non-existent shape");
	physics::AABB aabb{ a.instance->CastTo<physics::AABB>() };
	physics::AABB aabb2{ b.instance->CastTo<physics::AABB>() };
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (A.position.x + aabb.size.x <= B.position.x || A.position.x >= B.position.x + aabb2.size.x) return false;
	if (A.position.y + aabb.size.y <= B.position.y || A.position.y >= B.position.y + aabb2.size.y) return false;
	return true;
}

} // namespace collision

} // namespace ptgn