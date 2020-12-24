#pragma once

#include <engine/Include.h>

#include "components/TargetComponent.h"

ecs::Entity CreateBullet(V2_double position, V2_double target_position, ecs::Manager& manager) {
	//position.y -= 40;
	auto bullet = manager.CreateEntity();
	auto speed = 3;
	auto& target = bullet.AddComponent<TargetComponent>(target_position, speed);
	auto scale = V2_double{ 3, 3 };
	auto& transform = bullet.AddComponent<TransformComponent>(position);
	auto sprite_size = V2_double{ 5, 5 };
	V2_int collider_size = sprite_size * scale;
	bullet.AddComponent<CollisionComponent>(position, collider_size);
	auto& rb = bullet.AddComponent<RigidBodyComponent>(RigidBody{ DRAGLESS, GRAVITY });
	rb.rigid_body.acceleration = (target.target_position - transform.position).Normalized() * target.approach_speed;
	auto& sprite = bullet.AddComponent<SpriteComponent>("./resources/textures/bullet.png", scale, sprite_size);
	bullet.AddComponent<RenderComponent>();
	bullet.AddComponent<LifetimeComponent>(5);
	return bullet;
}