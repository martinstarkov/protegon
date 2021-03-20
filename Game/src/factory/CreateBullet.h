#pragma once

#include <engine/Include.h>

#include "components/TargetComponent.h"

inline ecs::Entity CreateBullet(V2_double position, ecs::Entity& target, ecs::Manager& manager) {
	//position.y -= 40;
	auto bullet{ manager.CreateEntity() };
	auto speed{ 30 };
	assert(target.HasComponent<TransformComponent>() && "Target must have TransformComponent");
	assert(target.HasComponent<CollisionComponent>() && "Target must have CollisionComponent");
	auto& target_position{ target.GetComponent<TransformComponent>().position };
	auto& target_collider{ target.GetComponent<CollisionComponent>().collider };
	auto& target_component{ bullet.AddComponent<TargetComponent>(target, target_position, speed) };
	V2_double scale{ 3, 3 };
	V2_double sprite_size{ 5, 5 };
	V2_int collider_size{ sprite_size * scale };
	auto& rb{ bullet.AddComponent<RigidBodyComponent>(RigidBody{ { 0.05, 0.05 }, GRAVITY }) };
	rb.rigid_body.velocity = (target_position + target_collider.size / 2.0 - position).Normalized() * target_component.approach_speed;
	// For position of bullet, offset it by its size so it is centered.
	// Note: this must be done after acceleration is set, if it is set here.
	position -= collider_size / 2.0;
	auto& transform{ bullet.AddComponent<TransformComponent>(position) };
	auto& collider{ bullet.AddComponent<CollisionComponent>(position, collider_size) };
	collider.ignored_tag_types.push_back(69);
	auto& sprite{ bullet.AddComponent<SpriteComponent>("./resources/textures/bullet.png", scale, sprite_size) };
	bullet.AddComponent<RenderComponent>();
	bullet.AddComponent<TagComponent>(69);
	bullet.AddComponent<LifetimeComponent>(2);
	return bullet;
}