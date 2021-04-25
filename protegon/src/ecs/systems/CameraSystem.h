#pragma once

#include "ecs/ECS.h"
#include "ecs/components/TransformComponent.h"
#include "ecs/components/CameraComponent.h"
#include "ecs/components/ShapeComponent.h"

#include "event/InputHandler.h"

namespace engine {

class CameraSystem : public ecs::System<TransformComponent, CameraComponent> {
public:
	static inline Key ZOOM_IN_KEY{ Key::Q };
	static inline Key ZOOM_OUT_KEY{ Key::E };
	virtual void Update() override final {
		// Set last found primary camera as the active camera.
		ecs::Entity primary_entity{ ecs::null };
		CameraComponent* primary_camera{ nullptr };
		TransformComponent* primary_transform{ nullptr };
		for (auto [entity, transform, camera] : entities) {
			if (camera.primary) {
				primary_entity = entity;
				primary_transform = &transform;
				primary_camera = &camera;
			}
		}
		if (primary_entity != ecs::null && 
			primary_camera != nullptr &&
			primary_transform != nullptr) {
			auto& camera{ primary_camera->camera };
			// Update camera zoom.
			if (engine::InputHandler::KeyPressed(ZOOM_IN_KEY) && engine::InputHandler::KeyReleased(ZOOM_OUT_KEY)) {
				camera.scale += camera.zoom_speed * camera.scale;
				camera.ClampToBound();
			} else if (engine::InputHandler::KeyPressed(ZOOM_OUT_KEY) && engine::InputHandler::KeyReleased(ZOOM_IN_KEY)) {
				camera.scale -= camera.zoom_speed * camera.scale;
				camera.ClampToBound();
			}
			// Find size of camera's target object if relevant.
			V2_double size; 
			if (primary_entity.HasComponent<ShapeComponent>()) {
				auto& shape{ primary_entity.GetComponent<ShapeComponent>() };
				size = shape.GetSize();
			}
			camera.CenterOn(primary_transform->transform.position, size, primary_camera->display_index);
		}
	}
};

}