//#pragma once
//
//#include "ecs/ECS.h"
//#include "ecs/components/TransformComponent.h"
//#include "ecs/components/CameraComponent.h"
//#include "ecs/components/ShapeComponent.h"
//#include "event/InputHandler.h"
//
//namespace engine {
//
//class CameraSystem : public ecs::System<TransformComponent, CameraComponent> {
//public:
//	static inline Key ZOOM_IN_KEY{ Key::Q };
//	static inline Key ZOOM_OUT_KEY{ Key::E };
//	virtual void Update() override final {
//		for (auto [entity, transform, camera] : entities) {
//			if (camera.primary) {
//				// Update camera zoom.
//				if (engine::InputHandler::KeyPressed(ZOOM_IN_KEY) &&
//					engine::InputHandler::KeyReleased(ZOOM_OUT_KEY)) {
//					camera.camera.scale += camera.camera.zoom_speed * camera.camera.scale;
//					camera.camera.ClampToBound();
//				} else if (engine::InputHandler::KeyPressed(ZOOM_OUT_KEY) &&
//						   engine::InputHandler::KeyReleased(ZOOM_IN_KEY)) {
//					camera.camera.scale -= camera.camera.zoom_speed * camera.camera.scale;
//					camera.camera.ClampToBound();
//				}
//				// Find size of camera's target object if relevant.
//				V2_double size;
//				if (entity.HasComponent<ShapeComponent>()) {
//					auto& shape{ entity.GetComponent<ShapeComponent>() };
//					size = shape.GetSize();
//				}
//				camera.camera.CenterOn(transform.transform.position, size);
//				break;
//			}
//		}
//	}
//};
//
//} // namespace engine