#pragma once

#include "System.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"

#include "renderer/TextureManager.h"
#include "renderer/FontManager.h"
#include "renderer/AABB.h"

#include "utils/Vector2.h"

class UIListener : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			auto surface = AABB{ transform.position, size.size };
			V2_double mouse_position = InputHandler::GetMousePosition();
			bool hovering = math::PointVsAABB(mouse_position, surface);
			if (hovering) {
				if (InputHandler::MouseReleased(MouseButton::LEFT)) {
					ui->Hover();
				} else if (!ui->IsActive() && InputHandler::MousePressed(MouseButton::LEFT)) {
					ui->Activate(mouse_position - transform.position);
					if (ui->HasInvoke()) {
						EventHandler::Invoke(entity, entity);
					}
				}
			} else {
				ui->ResetBackgroundColor();
			}
			/*if (ui.element.interacting && InputHandler::MousePressed(MouseButton::LEFT)) {
				transform.position = mouse_position - ui.element.mouse_offset;
			} else {
				render_component.color = ui.element.background_color;
			}*/
		}
	}
};

class UIRenderer : public ecs::System<UIComponent, TransformComponent, SizeComponent, RenderComponent> {
public:
	virtual void Update() override final {
		using namespace engine;
		for (auto [entity, ui, transform, size, render_component] : entities) {
			TextureManager::DrawSolidRectangle(transform.position, size.size, ui->GetBackgroundColor());
			if (ui->HasText()) {
				FontManager::Draw(ui->GetText(), transform.position, size.size);
			}
		}
	}
};