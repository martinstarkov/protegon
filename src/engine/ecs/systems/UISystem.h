#pragma once

#include "System.h"

#include <engine/event/InputHandler.h>
#include <engine/renderer/TextureManager.h>

#include <engine/renderer/AABB.h>
#include <engine/utils/Vector2.h>

class UIListener : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			auto surface = AABB{ transform.position, size.size };
			V2_double mouse_position = InputHandler::GetMousePosition();
			if (math::PointVsAABB(mouse_position, surface) && InputHandler::MousePressed(MouseButton::LEFT)) {
				ui.element.interacting = true;
			} else if (InputHandler::MouseReleased(MouseButton::LEFT) && ui.element.interacting) {
				ui.element.interacting = false;
			}
			if (ui.element.interacting) {
				if (InputHandler::MouseHeld(MouseButton::LEFT)) {
					transform.position = mouse_position - ui.element.mouse_offset;
					render_component.color = engine::GREEN;
				} else if (InputHandler::MousePressed(MouseButton::LEFT)) {
					render_component.color = engine::ORANGE;
					ui.element.mouse_offset = mouse_position - transform.position;
				}
			} else {
				render_component.color = ui.element.color;
			}
		}
	}
};

class UIRenderer : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			TextureManager::DrawSolidRectangle(transform.position, size.size, render_component.color);
		}
	}
};