#pragma once

#include <engine/Include.h>

ecs::Entity CreatePlayer(V2_double position, V2_int size, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	V2_double player_acceleration = { 2, 2 };
	auto scale = V2_int{ 5, 5 };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<InputComponent>();
	entity.AddComponent<PlayerController>(player_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, GRAVITY, 5, abs(player_acceleration) + abs(GRAVITY) });
	
	entity.AddComponent<SpriteSheetComponent>();
	//entity.AddComponent<StateMachineComponent>(entity, RawStateMachineMap{ { "walkStateMachine", new WalkStateMachine("idle") }, { "jumpStateMachine", new JumpStateMachine("grounded") }});
	entity.AddComponent<DirectionComponent>();
	entity.AddComponent<CollisionComponent>(position, V2_int{ 15, 21 } *scale);
	auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/sprite_test.png", scale);
	sprite.sprite_map.AddAnimation("walking_left", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 7, 0 });
	entity.AddComponent<AnimationComponent>("walking_left", 0.1);
	/*entity.AddComponent<CollisionComponent>(position, V2_int{ 28, 46 } * scale);
	auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/sprite_sheet_example.png", scale);
	sprite.sprite_map.AddAnimation("blonde_walking_left", engine::Animation{ { 44, 65 }, { 52, 62 }, { 56, 81 }, 5, 2 });
	entity.AddComponent<AnimationComponent>("blonde_walking_left", 0.1);*/
	
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CameraComponent>(engine::Camera{}, true);
	return entity;
}