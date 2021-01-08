#pragma once

#include <engine/Include.h>

ecs::Entity CreateHopper(V2_double position, V2_double size, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	V2_double hopper_acceleration = { 2, 4 };
	size = V2_int{ 20 * 2, 23 * 2 };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<PlayerController>(hopper_acceleration);
	entity.AddComponent<SizeComponent>(size);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, V2_double{ 0, 9.81 }, 5 });
	entity.AddComponent<CollisionComponent>(position, size);
	//auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/hopper.png", size);
	//sprite.sprite_map.AddAnimation("idle", engine::Animation{ { 0, 0 }, size, V2_int{ 0, 0 }, 1, 0 });
	entity.AddComponent<DirectionComponent>();
	auto& render = entity.AddComponent<RenderComponent>();
	render.color = engine::ORANGE;
	entity.AddComponent<StateVectorComponent>();
	entity.AddComponent<EDFComponent>(6*9.81);
	entity.AddComponent<HopperComponent>(0.1);
	return entity;
}