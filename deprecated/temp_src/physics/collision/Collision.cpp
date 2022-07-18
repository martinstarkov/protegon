#include "Collision.h"

#include "renderer/Colors.h"

namespace ptgn {

namespace collision {

namespace internal {

CollisionCallback StaticCollisionDispatch
[static_cast<int>(physics::ShapeType::COUNT)]
[static_cast<int>(physics::ShapeType::COUNT)] = {
    { StaticCirclevsCircle, StaticCirclevsAABB },
    { StaticAABBvsCircle, StaticAABBvsAABB }
};

Manifold StaticAABBvsAABB(const V2_double& A_position,
						  const V2_double& B_position,
						  const V2_double& A_size,
						  const V2_double& B_size) {

	// Use center positions.
	const auto half_A{ A_size / 2.0 };
	const auto half_B{ B_size / 2.0 };

	auto top_left_A{ A_position };
	auto top_left_B{ B_position };

	top_left_A += half_A;
	top_left_B += half_B;

	const auto depth_x{ top_left_B.x - top_left_A.x };
	const auto penetration_x{ (half_B.x + half_A.x) - math::Abs(depth_x) };

	Manifold manifold;

	if (penetration_x <= 0) {
		return manifold;
	}

	const auto depth_y{ top_left_B.y - top_left_A.y };
	const auto penetration_y{ (half_B.y + half_A.y) - math::Abs(depth_y) };

	if (penetration_y <= 0) {
		return manifold;
	}

	if (penetration_x < penetration_y) {
		const auto sign_x{ math::Sign(depth_x) };
		manifold.penetration.x = penetration_x * sign_x;
		manifold.normal.x = sign_x;
		manifold.contact_point.x = top_left_A.x + (half_A.x * sign_x);
		manifold.contact_point.y = top_left_B.y;
	} else {
		const auto sign_y{ math::Sign(depth_y) };
		manifold.penetration.y = penetration_y * sign_y;
		manifold.normal.y = sign_y;
		manifold.contact_point.x = top_left_B.x;
		manifold.contact_point.y = top_left_A.y + (half_A.y * sign_y);
	}

	return manifold;
}

Manifold StaticCirclevsCircle(const V2_double& A_position,
							  const V2_double& B_position,
							  const V2_double& A_size,
							  const V2_double& B_size) {
	Manifold manifold;

	auto A_radius{ A_size.x };
	auto B_radius{ B_size.x };

	auto normal{ B_position - A_position };
	auto distance_squared{ normal.MagnitudeSquared() };
	auto sum_radius{ A_radius + B_radius };

	// Collision did not occur.
	if (distance_squared >= sum_radius * sum_radius) {
		return manifold;
	}

	// Cache division.
	auto distance{ std::sqrt(distance_squared) };

	V2_double contact_point;

	// Bias toward selecting A for exact overlap edge case.
	if (distance == 0.0) {
		manifold.normal = { 1, 0 };
		manifold.penetration = A_radius * manifold.normal;
		contact_point = A_position;
	} else {
		// Normalise collision vector.
		manifold.normal = normal / distance;
		// Find the amount by which circles overlap.
		manifold.penetration = (sum_radius - distance) * manifold.normal;
		// Find point of collision from A.
		contact_point = manifold.normal * A_radius + A_position;
	}

	manifold.contact_point = contact_point;
	return manifold;
}

Manifold StaticAABBvsCircle(const V2_double& A_position,
							const V2_double& B_position,
							const V2_double& A_size,
							const V2_double& B_size) {
	Manifold manifold;

	auto circle_radius{ B_size.x };
	auto center{ B_position };
	auto aabb_half_extents{ A_size / 2.0 };
	auto aabb_center{ A_position + aabb_half_extents };
	auto difference{ center - aabb_center };
	auto original_difference{ difference };
	auto clamped{ math::Clamp(difference, -aabb_half_extents, aabb_half_extents) };
	auto closest{ aabb_center + clamped };

	difference = closest - center;
	bool inside{ original_difference == clamped };

	if (difference.MagnitudeSquared() <= circle_radius * circle_radius) {

		manifold.normal = -difference.Identity();
		auto penetration{ circle_radius * math::Abs(difference.Normalize()) - math::Abs(difference) };
		manifold.penetration = math::Abs(penetration) * manifold.normal;
		manifold.contact_point = closest;

		if (inside) {

			manifold.normal = {};
			manifold.contact_point = B_position;

			if (original_difference.x >= 0) {
				manifold.normal.x = 1;
			} else {
				manifold.normal.x = -1;
			}
			if (original_difference.y >= 0) {
				manifold.normal.y = 1;
			} else {
				manifold.normal.y = -1;
			}

			auto penetration{ aabb_half_extents - math::Abs(original_difference) };

			if (penetration.x > penetration.y) {
				manifold.normal.x = 0;
			} else {
				manifold.normal.y = 0;
			}

			manifold.penetration = (penetration + circle_radius) * manifold.normal;
		}
	}
	return manifold;
}

Manifold StaticCirclevsAABB(const V2_double& A_position,
							const V2_double& B_position,
							const V2_double& A_size,
							const V2_double& B_size) {
	Manifold manifold{ StaticAABBvsCircle(B_position, A_position, B_size, A_size) };
	manifold.normal *= -1;
	manifold.penetration *= -1;
	return manifold;
}

} // namespace internal

Manifold StaticIntersection(const V2_double& A_position,
							const V2_double& B_position,
							const V2_double& A_size,
							const V2_double& B_size,
							const physics::ShapeType& a_type,
							const physics::ShapeType& b_type) {
	return internal::StaticCollisionDispatch[static_cast<int>(a_type)][static_cast<int>(b_type)](A_position, B_position, A_size, B_size);
}

void Clear(ecs::Manager& manager) {
	manager.ForEachEntityWith<component::Collider>([](ecs::Entity& entity, component::Collider& collider) {
		collider.Clear();
	});
}

void Update(ecs::Manager& manager, double dt) {
	// Static collision detection for objects which have moved due to sweeps (dynamic AABBs).
	// Important note: The static check mostly prevents objects from preferring to stay inside each other if both are dynamic (altough this still occurs in circumstances where the resolution would result in another static collision).
	//for (auto [entity, transform, shape] : static_check) {
	//	// Check if there is currently an overlap with any other collider.
	//	manager.ForEachEntityWith<component::Transform, component::Shape>([&](auto& entity2, auto& transform2, auto& shape2) {
	//		if (entity != entity2) { // Do not check against self.
	//			if (AABBVsAABB(transform.position, shape.instance->GetSize(), transform2.position, shape2.instance->GetSize())) {
	//				auto collision_depth = IntersectAABB(transform.position, shape.instance->GetSize(), transform2.position, shape2.instance->GetSize());
	//				if (!collision_depth.IsZero()) { // Static collision occured.
	//					// Resolve static collision.
	//					transform.position -= collision_depth;
	//				}
	//			}
	//		}
	//	});
	//}
	/*
	manager.ForEachEntityWith<component::Collider, component::Transform, component::Shape>([&](ecs::Entity& entityA, component::Collider& colliderA, component::Transform& transformA, component::Shape& shapeA) {
		manager.ForEachEntityWith<component::Collider, component::Transform, component::Shape>([&](ecs::Entity& entityB, component::Collider& colliderB, component::Transform& transformB, component::Shape& shapeB) {
			if (colliderA.collideable && colliderB.collideable && entityA != entityB) {
				auto manifold = collision::StaticIntersection(transformA, transformB, shapeA, shapeB);
				if (manifold.CollisionOccured()) {
					colliderA.manifolds.emplace_back(manifold);
					colliderB.manifolds.emplace_back(manifold);
					//rigid_bodyA.velocity = rigid_bodyA.velocity.DotProduct(normal_flip) * normal_flip;
					transformA.position -= manifold.penetration;
					// TODO: Move this out, into some resolve function with templates possibly? Component for resolving collisions?
					//if (entityA.HasComponent<component::RigidBody>()) {
						//auto& rigid_bodyA = entityA.GetComponent<component::RigidBody>();
						//auto normal_flip = manifold.normal.Flip();
					//};
				}
			}
		});
	});



	manager.ForEachEntityWith<component::Transform, component::Shape>([&](ecs::Entity& entity1, component::Transform& transform1, component::Shape& shape1) {
		manager.ForEachEntityWith<component::Transform, component::Shape>([&](ecs::Entity& entity2, component::Transform& transform2, component::Shape& shape2) {
			if (entity1 != entity2) {
				if (AABBVsAABB(transform1.position, shape1.instance->GetSize(), transform2.position, shape2.instance->GetSize())) {
					debug::PrintLine("Collision system failed");
				}
			}
		});
	});
	*/
}

void Resolve(ecs::Manager& manager) {
	// TODO: Figure out how to resolve different types of collisions. Lambdas with template args? Component approach?
}

bool AABBvsAABB(const V2_double& A_position,
				const V2_double& B_position,
				const V2_double& A_size,
				const V2_double& B_size) {
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (A_position.x + A_size.x <= B_position.x || A_position.x >= B_position.x + B_size.x) return false;
	if (A_position.y + A_size.y <= B_position.y || A_position.y >= B_position.y + B_size.y) return false;
	return true;
}

} // namespace collision

} // namespace ptgn