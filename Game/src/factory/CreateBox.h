#pragma once

#include <engine/Include.h>

ecs::Entity& CreateBox(ecs::Entity& entity, V2_double position, V2_int size, const char* path) {
	entity.AddComponent<RenderComponent>();
	//entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SizeComponent>(V2_double{ 32, 32 });
	entity.AddComponent<SpriteComponent>(path, V2_int{ 1, 1 }, size);
	entity.AddComponent<TransformComponent>(position);
	return entity;
}