#pragma once

#include <engine/Include.h>

ecs::Entity CreateHopper(V2_double position, ecs::Manager& manager, engine::Scene& scene) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<PlayerController>();
	Polygon polygon;
	auto size = V2_double{ 64, 64 };
	entity.AddComponent<SizeComponent>(size);
	std::vector<V2_double> vertices;
	vertices.resize(4);
	// Create hopper shape.
	vertices[0] = { -size.x / 4.0, -size.y / 2.0 };
	vertices[1] = { size.x / 4.0, -size.y / 2.0 };
	vertices[2] = { size.x / 2.0, size.y / 2.0 };
	vertices[3] = { -size.x / 2.0, size.y / 2.0 };
	polygon.Set(vertices);
	auto* b = new Body(&polygon, position + size / 2.0);
	b->name = 69; // Do not change, otherwise collisions will break.

	entity.AddComponent<RigidBodyComponent>(b);
	auto& render = entity.AddComponent<RenderComponent>(engine::ORANGE);

	auto camera_zoom = V2_double{ 0.5, 0.5 };
	auto& cc = entity.AddComponent<CameraComponent>(engine::Camera{ camera_zoom }, true);
	scene.SetCamera(cc.camera);
	entity.AddComponent<HopperComponent>();
	return entity;
}