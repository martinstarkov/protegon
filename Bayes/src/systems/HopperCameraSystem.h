#pragma once

#include <engine/Include.h>

#define SCALE_BOUNDARY V2_double{ 5, 5 } // +/- this much max to the scale when zooming
#define ZOOM_SPEED V2_double{ 0.1, 0.1 } // +/- this much max to the scale when zooming

class HopperCameraSystem : public ecs::System<CameraComponent> {
public:
	HopperCameraSystem(engine::Scene* scene) : scene{ scene } {}
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