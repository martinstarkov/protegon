#pragma once

#include <engine/Include.h>

ecs::Entity CreateBox(V2_double position, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	auto size = V2_double{ 32, 32 };
	entity.AddComponent<SizeComponent>(size);
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_int{ 32, 32 });
	entity.AddComponent<TransformComponent>(position);
	return entity;
}