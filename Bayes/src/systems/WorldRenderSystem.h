#pragma once

#include <engine/Include.h>

class WorldRenderSystem : public ecs::System<RenderComponent, RigidBodyComponent> {
public:
	WorldRenderSystem() = default;
	WorldRenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		if (scene) {
			auto camera = scene->GetCamera();
			V2_double offset = { 0, 0 };
			V2_double scale = { 1.0, 1.0 };
			if (camera) {
				offset = camera->offset;
				scale = camera->scale;
			}
			for (auto& [entity, render, rb] : entities) {
				auto vertices = rb.body->shape->GetVertices();
				for (auto vertex = 0; vertex < vertices->size(); ++vertex) {
					auto v1 = (rb.body->position - offset + rb.body->shape->GetRotationMatrix() * (*vertices)[vertex]) * scale;
					auto v2 = (rb.body->position - offset + rb.body->shape->GetRotationMatrix() * (*vertices)[(vertex + 1) % vertices->size()]) * scale;
					engine::TextureManager::DrawLine(v1, v2, render.color);
				}
			}
			for (auto [aabb, color] : DebugDisplay::rectangles()) {
				engine::TextureManager::DrawRectangle(
					Ceil((aabb.position - offset) * scale),
					Ceil(aabb.size * scale),
					color);
			}
			DebugDisplay::rectangles().clear();
			for (auto [position, vertices, rotation_matrix, color] : DebugDisplay::polygons()) {
				for (auto vertex = 0; vertex < vertices.size(); ++vertex) {
					auto v1 = (position - offset + rotation_matrix * vertices[vertex]) * scale;
					auto v2 = (position - offset + rotation_matrix * vertices[(vertex + 1) % vertices.size()]) * scale;
					engine::TextureManager::DrawLine(v1, v2, color);
				}
			}
			DebugDisplay::polygons().clear();
			for (auto [origin, destination, color] : DebugDisplay::lines()) {
				engine::TextureManager::DrawLine(
					Ceil((origin - offset) * scale),
					Ceil((destination - offset) * scale),
					color);
			}
			DebugDisplay::lines().clear();
			for (auto [center, radius, color] : DebugDisplay::circles()) {
				engine::TextureManager::DrawCircle(
					Ceil((center - offset) * scale),
					static_cast<int>(engine::math::FastRound(radius * scale.x)),
					color);
			}
			DebugDisplay::circles().clear();
		}

	}
private:
	engine::Scene* scene = nullptr;
};