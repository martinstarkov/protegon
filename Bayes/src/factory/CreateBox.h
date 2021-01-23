#pragma once

#include <engine/Include.h>

ecs::Entity CreateBox(V2_double position, V2_double size, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<SizeComponent>(size);
	entity.AddComponent<TransformComponent>(position);
	Polygon polygon;
	polygon.SetBox(size.x / 2.0, size.y / 2.0);
	auto* b = new Body(&polygon, position + size / 2.0);
	entity.AddComponent<RigidBodyComponent>(b);
	return entity;
}