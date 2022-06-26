#include "Collision.h"
#include "renderer/Colors.h"

#include "debugging/Debug.h"

namespace ptgn {

namespace collision {

namespace internal {

CollisionCallback StaticCollisionDispatch
[static_cast<int>(ptgn::physics::ShapeType::COUNT)]
[static_cast<int>(ptgn::physics::ShapeType::COUNT)] = {
    { StaticCirclevsCircle, StaticCirclevsAABB },
    { StaticAABBvsCircle, StaticAABBvsAABB }
};

Manifold StaticAABBvsAABB(const component::Transform& A,
						  const component::Transform& B,
						  const component::Shape& a,
						  const component::Shape& b) {
	assert(a.instance != nullptr && "Cannot generate manifold for non-existent shape");
	assert(b.instance != nullptr && "Cannot generate manifold for non-existent shape");
	
	physics::Rectangle aabb_A{ a.instance->CastTo<physics::Rectangle>() };
	physics::Rectangle aabb_B{ b.instance->CastTo<physics::Rectangle>() };

	// Use center positions.
	const auto half_A{ aabb_A.size / 2.0 };
	const auto half_B{ aabb_B.size / 2.0 };

	auto top_left_A{ A.position };
	auto top_left_B{ B.position };

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

Manifold StaticCirclevsCircle(const component::Transform& A,
							  const component::Transform& B,
							  const component::Shape& a,
							  const component::Shape& b) {
	assert(a.instance != nullptr && "Cannot generate manifold for destroyed shape");
	assert(b.instance != nullptr && "Cannot generate manifold for destroyed shape");

	Manifold manifold;

	physics::Circle circle_A{ a.instance->CastTo<physics::Circle>() };
	physics::Circle circle_B{ b.instance->CastTo<physics::Circle>() };

	auto normal{ B.position - A.position };
	auto distance_squared{ normal.MagnitudeSquared() };
	auto sum_radius{ circle_A.radius + circle_B.radius };

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
		manifold.penetration = circle_A.radius * manifold.normal;
		contact_point = A.position;
	} else {
		// Normalise collision vector.
		manifold.normal = normal / distance;
		// Find the amount by which circles overlap.
		manifold.penetration = (sum_radius - distance) * manifold.normal;
		// Find point of collision from A.
		contact_point = manifold.normal * circle_A.radius + A.position;
	}

	manifold.contact_point = contact_point;
	return manifold;
}

Manifold StaticAABBvsCircle(const component::Transform& A,
							const component::Transform& B,
							const component::Shape& a,
							const component::Shape& b) {
	assert(a.instance != nullptr && "Cannot generate manifold for destroyed shape");
	assert(b.instance != nullptr && "Cannot generate manifold for destroyed shape");

	physics::Rectangle aabb{ a.instance->CastTo<physics::Rectangle>() };
	physics::Circle circle{ b.instance->CastTo<physics::Circle>() };

	Manifold manifold;

	auto center{ B.position };
	auto aabb_half_extents{ aabb.size / 2.0 };
	auto aabb_center{ A.position + aabb_half_extents };
	auto difference{ center - aabb_center };
	auto original_difference{ difference };
	auto clamped{ math::Clamp(difference, -aabb_half_extents, aabb_half_extents) };
	auto closest{ aabb_center + clamped };

	difference = closest - center;
	bool inside{ original_difference == clamped };

	if (difference.MagnitudeSquared() <= circle.radius * circle.radius) {

		manifold.normal = -difference.Identity();
		auto penetration{ circle.radius * math::Abs(difference.Normalize()) - math::Abs(difference) };
		manifold.penetration = math::Abs(penetration) * manifold.normal;
		manifold.contact_point = closest;

		if (inside) {

			manifold.normal = {};
			manifold.contact_point = B.position;

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

			manifold.penetration = (penetration + circle.radius) * manifold.normal;
		}
	}
	return manifold;
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

void Update(ecs::Manager& manager, double dt) {

	// Vector containing entities which have changed positions during the current cycle.
	std::vector<std::tuple<ecs::Entity, component::Transform&, component::Shape&>> static_check;
	// Swept collision detection and resolution routine.
	manager.ForEachEntityWith<component::Transform, component::Shape>([&](auto& entity, auto& transform, auto& shape) {

		// Round the position to the nearest whole number, this ensures collision detection is precise and prevents tunneling. Very important.
		auto position = transform.position;//math::Round(transform.position);
		auto size = shape.instance->GetSize();

		// Do not check static entities against other entities but the other way around.
		if (entity.HasComponent<component::RigidBody>()) {
			auto& rb = entity.GetComponent<component::RigidBody>(); // Passed later to static check.
			// Vector of entities which the entity could potentially have collided with during its path.
			/*
			std::vector<ecs::Entity> broadphase_entities;
			auto [broadphase_position, broadphase_size] = GetBroadphaseBox(rb.velocity, position, size);
			manager.ForEachEntityWith<component::Transform, component::Shape>([&](auto& entity2, auto& transform2, auto& shape2) {
				if (entity != entity2) {
					if (AABBVsAABB(broadphase_position, broadphase_size, transform2.position, shape2.instance->GetSize())) {
						broadphase_entities.emplace_back(entity2);
					}
				}
			});
			*/
			// Vector of entities which collided with the swept path of the collider
			std::vector<Collision> collisions;
			// Temporary object used inside each loop for passing by reference.
			Collision info;
			// Second sweep (narrow phase) collision detection.
			manager.ForEachEntityWith<component::Transform, component::Shape>([&](auto& entity2, auto& target_transform, auto& target_shape) {
				if (entity != entity2) {
					V2_double target_velocity;
					if (entity2.HasComponent<component::RigidBody>()) {
						target_velocity = entity2.GetComponent<component::RigidBody>().velocity;
					}
					auto relative_velocity = rb.velocity - target_velocity;
					if (DynamicAABBVsAABB(dt, relative_velocity, position, size, target_transform.position, target_shape.instance->GetSize(), info.manifold)) {
						info.entity = entity2;
						collisions.emplace_back(info);
					}
				}
			});

			SortTimes(collisions);

			// A swept collision occurred.
			if (collisions.size() > 0) {

				// Store old velocity to see if it changes and complete second sweep.
				auto old_velocity = rb.velocity;

				// First sweep (narrow phase) collision resolution.
				for (auto& collision : collisions) {
					auto& target_transform = collision.entity.GetComponent<component::Transform>();
					auto& target_shape = collision.entity.GetComponent<component::Shape>();
					V2_double target_velocity;
					if (collision.entity.HasComponent<component::RigidBody>()) {
						target_velocity = collision.entity.GetComponent<component::RigidBody>().velocity;
					}
					auto relative_velocity = rb.velocity - target_velocity;
					if (ResolveDynamicAABBVsAABB(dt, relative_velocity, rb.velocity, position, size, target_transform.position, target_shape.instance->GetSize(), collision.manifold)) {
						// ... objects which were used to resolve the collision, not necessarily all touching objects.
					}
					// TODO: Limit collision coloring to only objects which touch the player.
					collision.entity.GetComponent<component::Color>() = color::RED;
				}

				// Velocity changed, complete a second sweep to ensure both axes have been swept.
				
				if (rb.velocity.x != old_velocity.x || rb.velocity.y != old_velocity.y) {
					// Clear first sweep collisions.
					collisions.clear();
					// Second sweep (narrow phase) collision detection.
					manager.ForEachEntityWith<component::Transform, component::Shape>([&](auto& entity2, auto& target_transform, auto& target_shape) {
						if (entity != entity2) {
							V2_double target_velocity;
							if (entity2.HasComponent<component::RigidBody>()) {
								target_velocity = entity2.GetComponent<component::RigidBody>().velocity;
							}
							auto relative_velocity = rb.velocity - target_velocity;
							if (DynamicAABBVsAABB(dt, relative_velocity, position, size, target_transform.position, target_shape.instance->GetSize(), info.manifold)) {
								info.entity = entity2;
								collisions.emplace_back(info);
							}
						}
					});
					if (collisions.size() > 0) {

						SortTimes(collisions);
						// Second sweep (narrow phase) collision resolution.
						for (auto& collision : collisions) {
							auto& target_transform = collision.entity.GetComponent<component::Transform>();
							auto& target_shape = collision.entity.GetComponent<component::Shape>();
							V2_double target_velocity;
							if (collision.entity.HasComponent<component::RigidBody>()) {
								target_velocity = collision.entity.GetComponent<component::RigidBody>().velocity;
							}
							auto relative_velocity = rb.velocity - target_velocity;
							ResolveDynamicAABBVsAABB(dt, relative_velocity, rb.velocity, position, size, target_transform.position, target_shape.instance->GetSize(), collision.manifold);
						}
					}
				}
			}
			// Update collider position with resolved velocity.
			position += rb.velocity * dt;
			// If position has changed, update it and add entity to static collision check list.
			if (transform.position != position) {
				transform.position = position;
				static_check.emplace_back(entity, transform, shape);
			}
		}
	});
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
					/*if (entityA.HasComponent<component::RigidBody>()) {
						auto& rigid_bodyA = entityA.GetComponent<component::RigidBody>();
						auto normal_flip = manifold.normal.Flip();
					};*/
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
	physics::Rectangle aabb{ a.instance->CastTo<physics::Rectangle>() };
	physics::Rectangle aabb2{ b.instance->CastTo<physics::Rectangle>() };
	// If any side of the aabb it outside the other aabb, there cannot be an overlap.
	if (A.position.x + aabb.size.x <= B.position.x || A.position.x >= B.position.x + aabb2.size.x) return false;
	if (A.position.y + aabb.size.y <= B.position.y || A.position.y >= B.position.y + aabb2.size.y) return false;
	return true;
}

} // namespace collision

} // namespace ptgn