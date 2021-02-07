#pragma once

#include "ecs/System.h"

#include "core/Scene.h"

#include "event/InputHandler.h"

#define SCALE_BOUNDARY V2_double{ 5, 5 } // +/- this much max to the scale when zooming
#define ZOOM_SPEED V2_double{ 0.1, 0.1 } // +/- this much max to the scale when zooming

class CameraSystem : public ecs::System<CameraComponent> {
public:
	CameraSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		assert(scene != nullptr && "Cannot update camera system without a valid scene");
		// Set last found primary camera as the active camera.
		ecs::Entity primary_entity = ecs::null;
		for (auto [entity, camera] : entities) {
			if (camera.primary) {
				scene->SetCamera(camera.camera);
				primary_entity = entity;
			}
		}
		if (primary_entity) {
			auto camera = scene->GetCamera();
			assert(camera != nullptr && "Scene camera undefined");
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
			V2_double sprite_scale{ 1.0, 1.0 }; 
			if (primary_entity.HasComponent<SizeComponent>()) {
				size = primary_entity.GetComponent<SizeComponent>().size;
			}
			if (primary_entity.HasComponent<CollisionComponent>()) {
				size = primary_entity.GetComponent<CollisionComponent>().collider.size;
			}
			V2_double pos{};
			if (primary_entity.HasComponent<TransformComponent>()) {
				pos = primary_entity.GetComponent<TransformComponent>().position;
			}
			if (primary_entity.HasComponent<RigidBodyComponent>()) {
				auto body = primary_entity.GetComponent<RigidBodyComponent>().body;
				if (body != nullptr) {
					pos = body->position;
				}
			}
			camera->Center(pos, size);
		}
	}
private:
	engine::Scene* scene = nullptr;
};