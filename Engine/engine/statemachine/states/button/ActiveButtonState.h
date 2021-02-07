#pragma once

#include "statemachine/State.h"

#include "event/EventHandler.h"
#include "event/InputHandler.h"
#include "ui/UIComponents.h"
#include "physics/collision/static/PointvsAABB.h"

constexpr double BUTTON_ACTIVE_TIME = 0.2;

class ActiveButtonState : public engine::State {
	virtual void OnEntry() override final {
		timer.Start();
		engine::EventHandler::Invoke(parent_entity);
	}
	virtual void Update() override final {
		if (timer.ElapsedSeconds() > BUTTON_ACTIVE_TIME) {
			assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update button without TransformComponent");
			assert(parent_entity.HasComponent<SizeComponent>() && "Cannot update button without SizeComponent");
			auto& transform = parent_entity.GetComponent<TransformComponent>();
			auto& size = parent_entity.GetComponent<SizeComponent>().size;
			auto surface = AABB{ transform.position, size };
			auto mouse_position = engine::InputHandler::GetMousePosition();
			auto hovering = engine::collision::PointvsAABB(mouse_position, surface);
			if (hovering) {
				if (engine::InputHandler::MousePressed(MouseButton::LEFT)) {
					parent_state_machine->SetState("focused");
				} else {
					parent_state_machine->SetState("hover");
				}
			} else {
				parent_state_machine->SetState("default");
			}
		}
		if (parent_entity.HasComponent<BackgroundColorComponent>()) {
			auto& background = parent_entity.GetComponent<BackgroundColorComponent>();
			if (parent_entity.HasComponent<ActiveColorComponent>()) {
				background.color = parent_entity.GetComponent<ActiveColorComponent>().color;
			} else {
				background.color = background.original_color;
			}
		}
	}
	engine::Timer timer;
};