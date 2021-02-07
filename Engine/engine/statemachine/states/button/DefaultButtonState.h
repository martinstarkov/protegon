#pragma once

#include "statemachine/State.h"

#include "ui/UIComponents.h"

#include "renderer/AABB.h"
#include "event/InputHandler.h"
#include "physics/collision/static/PointvsAABB.h"

class DefaultButtonState : public engine::State {
	virtual void Update() override final {
		assert(parent_entity.HasComponent<TransformComponent>() && "Cannot update button without TransformComponent");
		assert(parent_entity.HasComponent<SizeComponent>() && "Cannot update button without SizeComponent");
		auto& transform = parent_entity.GetComponent<TransformComponent>();
		auto& size = parent_entity.GetComponent<SizeComponent>().size;
		auto surface = AABB{ transform.position, size };
		auto mouse_position = engine::InputHandler::GetMousePosition();
		auto hovering = engine::collision::PointvsAABB(mouse_position, surface);
		if (hovering && engine::InputHandler::MouseReleased(MouseButton::LEFT)) {
			parent_state_machine->SetState("hover");
		}
		if (parent_entity.HasComponent<BackgroundColorComponent>()) {
			auto& background = parent_entity.GetComponent<BackgroundColorComponent>();
			background.color = background.original_color;
		}
	}
};