#pragma once

#include <engine/Include.h>

ecs::Entity& CreateBox(ecs::Entity& entity, V2_double position, V2_int size, const char* path) {
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SizeComponent>(size);
	//entity.AddComponent<SpriteComponent>(path, V2_int{ 1, 1 }, size);
	entity.AddComponent<TransformComponent>(position);
	return entity;
}

ecs::Entity CreateBox(ecs::Manager& manager, V2_double position, V2_int size, const char* path) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SizeComponent>(size);
	//entity.AddComponent<SpriteComponent>(path, V2_int{ 1, 1 }, size);
	entity.AddComponent<TransformComponent>(position);
	return entity;
}