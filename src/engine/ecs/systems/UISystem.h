#pragma once

#include "System.h"

#include <engine/event/InputHandler.h>

#include <engine/renderer/TextureManager.h>
#include <engine/renderer/FontManager.h>
#include <engine/renderer/AABB.h>

#include <engine/utils/Vector2.h>

class UIListener : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			auto surface = AABB{ transform.position, size.size };
			V2_double mouse_position = InputHandler::GetMousePosition();
			if (!ui.element.interacting && InputHandler::MousePressed(MouseButton::LEFT) && math::PointVsAABB(mouse_position, surface)) {
				ui.element.interacting = true;
				ui.element.mouse_offset = mouse_position - transform.position;
				render_component.color = engine::GREEN;
				EventHandler::Invoke(entity, entity, *ui.manager, GetManager());
			} else if (InputHandler::MouseReleased(MouseButton::LEFT) && ui.element.interacting) {
				ui.element.interacting = false;
				ui.element.mouse_offset = {};
			}
			if (ui.element.interacting && InputHandler::MousePressed(MouseButton::LEFT)) {
				transform.position = mouse_position - ui.element.mouse_offset;
			} else {
				render_component.color = ui.element.background_color;
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
			if (ui.element.font_text != "") {
				FontManager::Draw(ui.element.font_text, transform.position, size.size);
			}
		}
	}
};