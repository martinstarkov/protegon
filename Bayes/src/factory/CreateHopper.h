#pragma once

#include <engine/Include.h>

ecs::Entity CreateHopper(V2_double position, V2_double size, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	V2_double hopper_acceleration = { 2, 4 };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<PlayerController>(hopper_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, V2_double{ 0, 9.81 }, 5 });
	entity.AddComponent<CollisionComponent>(position, size);
	entity.AddComponent<SpriteComponent>("./resources/textures/hopper.png", V2_int{ 20 * 2, 23 * 2 });
	entity.AddComponent<DirectionComponent>();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<StateVectorComponent>();
	entity.AddComponent<EDFComponent>(6*9.81);
	return entity;
}