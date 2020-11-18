#pragma once

#include "System.h"

#include "core/Scene.h"

#include "event/InputHandler.h"

#define SCALE_BOUNDARY V2_double{ 0.3, 0.3 } // +/- this much max to the scale when zooming
#define ZOOM_SPEED V2_double{ 0.01, 0.01 } // +/- this much max to the scale when zooming

class CameraSystem : public ecs::System<TransformComponent, CameraComponent> {
public:
	CameraSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		assert(scene != nullptr && "Cannot update camera system without a valid scene");
		auto camera = scene->GetCamera();
		if (camera) {
			// Set last found primary camera as the active camera.
			ecs::Entity primary_entity = ecs::null;
			for (auto [entity, transform, camera] : entities) {
				if (camera.primary) {
					scene->SetCamera(camera.camera);
					primary_entity = entity;
				}
			}
			if (primary_entity) {
				// Update scale first.
				if (engine::InputHandler::KeyPressed(Key::Q) && engine::InputHandler::KeyReleased(Key::E)) {
					camera->scale *= (V2_double{ 1.0, 1.0 } + ZOOM_SPEED);
					camera->LimitScale(SCALE_BOUNDARY);
				} else if (engine::InputHandler::KeyPressed(Key::E) && engine::InputHandler::KeyReleased(Key::Q)) {
					camera->scale *= (V2_double{ 1.0, 1.0 } - ZOOM_SPEED);
					camera->LimitScale(SCALE_BOUNDARY);
				}
				// Then update offset.
				V2_double size{};
				if (primary_entity.HasComponent<SpriteComponent>()) {
					size = primary_entity.GetComponent<SpriteComponent>().source.size;
				}
				if (primary_entity.HasComponent<CollisionComponent>()) {
					size = primary_entity.GetComponent<CollisionComponent>().collider.size;
				}
				camera->Center(primary_entity.GetComponent<TransformComponent>().position, size);
			}
		}
	}
private:
	engine::Scene* scene = nullptr;
};