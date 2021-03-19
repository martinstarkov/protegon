#pragma once

#include <engine/Include.h>

inline ecs::Entity CreatePlayer(V2_double position, const V2_int& size, ecs::Manager& manager) {
	auto& scene{ engine::Scene::Get() };
	auto entity{ manager.CreateEntity() };
	V2_double player_acceleration{ 3, 3 };
	V2_double scale{ 3, 3 };
	position = { 0, 0 };
	auto gravity{ GRAVITY };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<InputComponent>();
	entity.AddComponent<PlayerController>(player_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, gravity, 1, 0.8, player_acceleration });
	
	entity.AddComponent<SpriteSheetComponent>();
	auto& sm{ entity.AddComponent<StateMachineComponent>() };
	sm.AddStateMachine<WalkStateMachine>("walk_state_machine", entity);
	entity.AddComponent<DirectionComponent>();
	V2_int collider_size = V2_double{ 15, 21 } * scale;
	auto& collider{ entity.AddComponent<CollisionComponent>(position, collider_size) };
	collider.ignored_tag_types.emplace_back(130);
	collider.ignored_tag_types.emplace_back(69);
	auto& sprite{ entity.AddComponent<SpriteComponent>("./resources/textures/gabe-idle-run.png", scale) };
	sprite.sprite_map.AddAnimation("idle", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 1, 0 });
	sprite.sprite_map.AddAnimation("walk", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 7, 0 });
	sprite.sprite_map.AddAnimation("run", engine::Animation{ { 0, 0 }, { 24, 24 }, V2_int{ 5, 3 }, 7, 0 });
	entity.AddComponent<AnimationComponent>("idle", 0.1);
	auto& render{ entity.AddComponent<RenderComponent>(engine::DARK_GREEN) };
	V2_double camera_zoom{ 0.5, 0.5 };
	auto& cc{ entity.AddComponent<CameraComponent>(engine::Camera{ camera_zoom }, true) };
	scene.SetCamera(cc.camera);
	return entity;
}