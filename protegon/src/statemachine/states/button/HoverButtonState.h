//#pragma once
//
//#include "statemachine/State.h"
//
//#include "event/InputHandler.h"
//#include "ui/UIComponents.h"
//#include "physics/collision/static/PointvsAABB.h"
//
//namespace engine {
//
//class HoverButtonState : public engine::State {
//	virtual void Update() override final {
//		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update button without TransformComponent");
//		assert(parent_entity.HasComponent<SizeComponent>() && "Cannot update button without SizeComponent");
//		auto& transform = parent_entity.GetComponent<TransformComponent>();
//		auto& size = parent_entity.GetComponent<SizeComponent>().size;
//		auto surface = AABB{ transform.position, size };
//		auto mouse_position = engine::InputHandler::GetMousePosition();
//		auto hovering = engine::collision::PointvsAABB(mouse_position, surface);
//
//		if (parent_entity.HasComponent<BackgroundColorComponent>()) {
//			auto& background = parent_entity.GetComponent<BackgroundColorComponent>();
//			if (parent_entity.HasComponent<HoverColorComponent>()) {
//				background.color = parent_entity.GetComponent<HoverColorComponent>().color;
//			} else {
//				background.color = background.original_color;
//			}
//		}
//		if (hovering) {
//			if (engine::InputHandler::MousePressed(MouseButton::LEFT) || engine::InputHandler::MouseHeld(MouseButton::LEFT) || engine::InputHandler::MouseHeldFor(MouseButton::LEFT, 1)) {
//				parent_state_machine->SetState("focused");
//				return;
//			}
//		} else {
//			parent_state_machine->SetState("default");
//			return;
//		}
//	}
//};
//
//} // namespace engine