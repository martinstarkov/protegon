#pragma once

#include "System.h"

#include <engine/event/InputHandler.h>
#include <engine/renderer/TextureManager.h>

#include <engine/renderer/AABB.h>

class UIListener : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			auto surface = AABB{ transform.position, size.size };
			if (math::PointVsAABB(InputHandler::GetMousePosition(), surface)) {
				render_component.color = engine::GREEN;
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