#pragma once

#include "statemachine/State.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"
#include "ui/UIComponents.h"
#include "physics/collision/static/PointvsAABB.h"

class FocusedButtonState : public engine::State {
	virtual void OnEntry() override final {
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update button without TransformComponent");
		auto& transform = parent_entity.GetComponent<TransformComponent>();
		auto mouse_position = engine::InputHandler::GetMousePosition();
		if (parent_entity.HasComponent<MouseOffsetComponent>()) {
			parent_entity.GetComponent<MouseOffsetComponent>().offset = mouse_position - transform.position;
		}
	}
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update button without TransformComponent");
		assert(parent_entity.HasComponent<SizeComponent>() && "Cannot update button without SizeComponent");
		auto& transform = parent_entity.GetComponent<TransformComponent>();
		auto& size = parent_entity.GetComponent<SizeComponent>().size;
		auto surface = AABB{ transform.position, size };
		auto mouse_position = engine::InputHandler::GetMousePosition();
		auto hovering = engine::collision::PointvsAABB(mouse_position, surface);
		if (hovering) {
			if (engine::InputHandler::MouseReleased(MouseButton::LEFT)) {
				parent_state_machine->SetState("active");
			}
		} else {
			parent_state_machine->SetState("default");
		}
		if (parent_entity.IsAlive() && parent_entity.HasComponent<BackgroundColorComponent>()) {
			auto& background = parent_entity.GetComponent<BackgroundColorComponent>();
			if (parent_entity.HasComponent<FocusedColorComponent>()) {
				background.color = parent_entity.GetComponent<FocusedColorComponent>().color;
			} else {
				background.color = background.original_color;
			}
		}
	}
};