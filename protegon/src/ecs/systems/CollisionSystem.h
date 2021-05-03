//#pragma once
//
//#include "ecs/System.h"
//
//#include <algorithm> // std::sort, std::find
//#include <vector> // std::vector
//
//#include "physics/Collision.h"
//#include "physics/collision/dynamic/DynamicAABBvsAABB.h"
//#include "physics/collision/static/AABBvsAABB.h"
//
//// TODO: Figure out relative velocities for two dynamic blocks.
//// TODO: Figure out how to resolve out all static collision in one frame.
//// TODO: Fix entity destruction.
//// TODO: Instead of returning list of collided objects make the collision call a
//// function on the collided entity right away (this gives for e.g. smoother mining).
//
//// Sort times in order of lowest time first, if times are equal prioritize diagonal collisions first.
//static void SortTimes(std::vector<Collision>& collisions) {
//	if (collisions.size() > 1) {
//		std::sort(collisions.begin(), collisions.end(), [](const Collision& a, const Collision& b) {
//			if (a.manifold.time != b.manifold.time) {
//				return a.manifold.time < b.manifold.time;
//			} else {
//				return a.manifold.normal.MagnitudeSquared() < b.manifold.normal.MagnitudeSquared();
//			}
//		});
//	}
//}
//
//class empty {
//public:
//	inline void EmptyFunction(ecs::Entity& e, Collision& c) {}
//};
//
//template <typename S = void, typename T>
//static std::vector<std::pair<ecs::Entity, Collision>> CollisionRoutine(std::vector<T>& entities1, std::vector<T>& entities2) {
//	std::vector<std::pair<ecs::Entity, Collision>> final_collisions;
//	// Color reset.
//	for (auto [entity, transform, collision] : entities2) {
//		if (entity.HasComponent<RenderComponent>()) {
//			entity.GetComponent<RenderComponent>().ResetColor();
//		}
//	}
//	// Vector containing entities which have changed positions during the current cycle.
//	std::vector<std::tuple<std::vector<ecs::Entity>, ecs::Entity, TransformComponent&, RigidBodyComponent&, CollisionComponent&>> static_check;
//	static_check.reserve(entities1.size()); // Reserve maximum possible capacity of moving entities.
//	// Swept collision detection and resolution routine.
//	for (auto [entity, transform, collision_component] : entities1) {
//			auto& collider = collision_component.collider;
//			// Round/Floor the position to the nearest whole number, this ensures collision detection is precise and prevents tunneling. Very important.
//			collider.position = Floor(transform.position);
//
//			// Do not check static entities against other entities but the other way around.
//			if (entity.HasComponent<RigidBodyComponent>()) {
//				auto& rigid_body = entity.GetComponent<RigidBodyComponent>(); // Passed later to static check.
//				auto& rb = rigid_body.rigid_body; // Makes code more readable as RigidBodyComponent is just a wrapper around RigidBody.
//				// Vector of entities which the entity could potentially have collided with during its path.
//				std::vector<ecs::Entity> broadphase_entities;
//				broadphase_entities.reserve(100); // Reserve large capacity of entities for possible collisions.
//				auto broadphase = collider.GetBroadphaseBox(rb.velocity);
//				for (auto [entity2, transform2, collision2] : entities2) {
//					bool skip = false;
//					// Do not check against self.
//					// Do not check against excluded entity tags (both of the first and second entity, i.e. check both excluded tag lists).
//					if (entity == entity2 || engine::HasExcludedTag(entity, collision2.ignored_tag_types) || engine::HasExcludedTag(entity2, collision_component.ignored_tag_types)) {
//						skip = true;
//					}
//					// If none of the above skip conditions are met, proceed with collision check.
//					if (!skip && engine::collision::AABBvsAABB(broadphase, collision2.collider)) {
//						broadphase_entities.emplace_back(entity2);
//					}
//				}
//				// Vector of entities which collided with the swept path of the collider
//				std::vector<Collision> collisions;
//				collisions.reserve(broadphase_entities.size());
//				// Temporary object used inside each loop for passing by reference.
//				Collision info;
//				// Second sweep (narrow phase) collision detection.
//				for (auto& entity2 : broadphase_entities) {
//					auto& target_collider = entity2.GetComponent<CollisionComponent>().collider;
//					if (engine::collision::DynamicAABBvsAABB(rb.velocity, collider, target_collider, info.manifold)) {
//						info.entity = entity2;
//						collisions.emplace_back(info);
//					}
//				}
//				// A swept collision occurred.
//				if (collisions.size() > 0) {
//					SortTimes(collisions);
//
//					// Store old velocity to see if it changes and complete second sweep.
//					auto old_velocity = rb.velocity;
//
//					// First sweep (narrow phase) collision resolution.
//					for (auto& collision : collisions) {
//						auto& target_collider{ collision.entity.GetComponent<CollisionComponent>().collider };
//						if (engine::collision::ResolveDynamicAABBvsAABB(rb.velocity, collider, target_collider, collision.manifold)) {
//							// ... objects which were used to resolve the collision, not necessarily all touching objects.
//						}
//						if (collision.manifold.time == 0 && !collision.manifold.normal.IsZero()) {
//							// Call custom function for collision
//							if constexpr (!std::is_same_v<S, void>) {
//								final_collisions.emplace_back(entity, collision);
//							} else {
//								if (collision.entity.HasComponent<RenderComponent>()) {
//									collision.entity.GetComponent<RenderComponent>().color = engine::RED;
//								}
//								auto& rigid_body{ entity.GetComponent<RigidBodyComponent>().rigid_body };
//								if (collision.manifold.normal.y != 0) {
//									rigid_body.acceleration.y = 0;
//									//rb.velocity.x *= 1.0 - rb.drag.x;
//								}
//								if (collision.manifold.normal.x != 0) {
//									rigid_body.acceleration.x = 0;
//									//rb.velocity.y *= 1.0 - rb.drag.y;
//								}
//							}
//						}
//					}
//
//					// Velocity changed, complete a second sweep to ensure both axes have been swept.
//					if (rb.velocity.x != old_velocity.x || rb.velocity.y != old_velocity.y) {
//						// Clear first sweep collisions.
//						collisions.clear();
//						// Second sweep (narrow phase) collision detection.
//						for (auto& entity2 : broadphase_entities) {
//							auto& target_collider{ entity2.GetComponent<CollisionComponent>().collider };
//								if (engine::collision::DynamicAABBvsAABB(rb.velocity, collider, target_collider, info.manifold)) {
//									info.entity = entity2;
//									collisions.emplace_back(info);
//								}
//						}
//						if (collisions.size() > 0) {
//
//							SortTimes(collisions);
//							// Second sweep (narrow phase) collision resolution.
//							for (auto& collision : collisions) {
//								auto& target_collider = collision.entity.GetComponent<CollisionComponent>().collider;
//								engine::collision::ResolveDynamicAABBvsAABB(rb.velocity, collider, target_collider, collision.manifold);
//							}
//						}
//					}
//				}
//				// Update collider position with resolved velocity.
//				collider.position += rb.velocity;
//				// If position has changed, update it and add entity to static collision check list.
//				if (transform.position != collider.position) {
//					transform.position = collider.position;
//					static_check.emplace_back(broadphase_entities, entity, transform, rigid_body, collision_component);
//				}
//			}
//	}
//	// Static collision detection for objects which have moved due to sweeps (dynamic AABBs).
//	// Important note: The static check mostly prevents objects from preferring to stay inside each other if both are dynamic (altough this still occurs in circumstances where the resolution would result in another static collision).
//	for (auto [broadphase_entities, entity, transform, rigid_body, collision_component] : static_check) {
//		auto& collider{ collision_component.collider };
//			// Check if there is currently an overlap with any other collider.
//			for (auto& entity2 : broadphase_entities) {
//					auto& transform2{ entity2.GetComponent<TransformComponent>() };
//					auto& collision2{ entity2.GetComponent<CollisionComponent>() };
//					bool skip{ false };
//					// Do not check against self.
//					// Do not check against excluded entity tags.
//					if (entity == entity2 || engine::HasExcludedTag(entity, collision2.ignored_tag_types) || engine::HasExcludedTag(entity2, collision_component.ignored_tag_types)) {
//						skip = true;
//					}
//					auto& oCollider{ collision2.collider };
//					if (!skip && engine::collision::AABBvsAABB(collider, oCollider)) {
//						V2_double normal;
//						auto collision_depth{ engine::collision::IntersectionAABBvsAABB(collider, oCollider, normal) };
//						if (!collision_depth.IsZero()) { // Static collision occured.
//							if constexpr (!std::is_same_v<S, void>) {
//								Collision collision{ entity2, CollisionManifold{ normal } };
//								final_collisions.emplace_back(entity, collision);
//							} else {
//								// Resolve static collision.
//								collider.position -= collision_depth;
//							}
//						}
//					}
//			}
//			// Keep transform position updated.
//			transform.position = collider.position;
//	}
//	return final_collisions;
//}
//
//class CollisionSystem : public ecs::System<TransformComponent, CollisionComponent> {
//public:
//	virtual void Update() override final {
//		CollisionRoutine(entities, entities);
//		GetManager().Refresh();
//	}
//private:
//};