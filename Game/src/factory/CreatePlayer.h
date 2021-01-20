#pragma once

#include <engine/Include.h>

ecs::Entity CreatePlayer(V2_double position, V2_int size, ecs::Manager& manager, engine::Scene& scene) {
	auto entity = manager.CreateEntity();
	V2_double player_acceleration = { 3, 0 };
	auto scale = V2_double{ 3, 3 };
	position = { 0, 0 };
	auto gravity = GRAVITY;
	//gravity.y += 3;
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<InputComponent>();
	entity.AddComponent<PlayerController>(player_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, gravity, 1, 0.8, player_acceleration });
	
	entity.AddComponent<SpriteSheetComponent>();
	auto& sm = entity.AddComponent<StateMachineComponent>();
	sm.AddStateMachine<WalkStateMachine>("walk_state_machine", entity);
	entity.AddComponent<DirectionComponent>();
	V2_int collider_size = V2_double{ 15, 21 } * scale;
	auto& collider = entity.AddComponent<CollisionComponent>(position, collider_size);
	collider.ignored_tag_types.emplace_back(130);
	collider.ignored_tag_types.emplace_back(69);
	auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/gabe-idle-run.png", scale);
	sprite.sprite_map.AddAnimation("idle", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 1, 0 });
	sprite.sprite_map.AddAnimation("walk", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 7, 0 });
	sprite.sprite_map.AddAnimation("run", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 7, 0 });
	entity.AddComponent<AnimationComponent>("idle", 0.1);
	/*entity.AddComponent<CollisionComponent>(position, V2_int{ 28, 46 } * scale);
	auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/sprite_sheet_example.png", scale);
	sprite.sprite_map.AddAnimation("blonde_walking_left", engine::Animation{ { 44, 65 }, { 52, 62 }, { 56, 81 }, 5, 2 });
	entity.AddComponent<AnimationComponent>("blonde_walking_left", 0.1);*/

	/*entity.AddComponent<CollisionComponent>(position, V2_int{ 28, 50 } *scale);
	auto& sprite = entity.AddComponent<SpriteComponent>("./resources/textures/sprite_sheet_example.png", scale);
	sprite.sprite_map.AddAnimation("blonde_walking_left", engine::Animation{ { 44, 449 }, { 52, 62 }, { 56, 461 }, 7, 2 });
	entity.AddComponent<AnimationComponent>("blonde_walking_left", 0.1);*/
	
	auto& render = entity.AddComponent<RenderComponent>(engine::DARK_GREEN);
	auto camera_zoom = V2_double{ 0.5, 0.5 };
	auto& cc = entity.AddComponent<CameraComponent>(engine::Camera{ camera_zoom }, true);
	scene.SetCamera(cc.camera);
	return entity;
}