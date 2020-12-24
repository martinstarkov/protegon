#pragma once

#include <engine/Include.h>

ecs::Entity CreateBox(V2_double position, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, V2_double{ 32, 32 });
	entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_int{ 1, 1 }, V2_int{ 32, 32 });
	entity.AddComponent<TransformComponent>(position);
	return entity;
}