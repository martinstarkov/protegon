#pragma once

#include <engine/Include.h>

ecs::Entity CreateBox(ecs::Entity& entity, V2_double position, V2_int size) {
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SizeComponent>(V2_double{ 32, 32 });
	entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_int{ 1, 1 }, size);
	entity.AddComponent<TransformComponent>(position);
	return entity;
}

ecs::Entity CreateBox(V2_int position, V2_int size, ecs::Manager& manager, const char* path) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SizeComponent>(size);
	entity.AddComponent<SpriteComponent>(path, V2_int{ 1, 1 }, size);
	entity.AddComponent<TransformComponent>(position);
	return entity;
}