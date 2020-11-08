#pragma once

#include "System.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"

#include "renderer/TextureManager.h"
#include "renderer/FontManager.h"
#include "renderer/AABB.h"

#include "utils/Vector2.h"

#include "ui/UIComponents.h"

class UIButtonListener : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, StateComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, transform, size, background, state] : entities) {
			auto surface = AABB{ transform.position, size.size };
			V2_double mouse_position = engine::InputHandler::GetMousePosition();
			bool hovering = engine::math::PointVsAABB(mouse_position, surface);
			if (hovering) {
				if (engine::InputHandler::MouseReleased(engine::MouseButton::LEFT)) {
					state.state = UIInteractionState::HOVER;
					if (entity.HasComponent<HoverColorComponent>()) {
						background.color = entity.GetComponent<HoverColorComponent>().color;
					} else {
						background.color = background.original_color;
					}
				} else if (engine::InputHandler::MousePressed(engine::MouseButton::LEFT) && state.state != UIInteractionState::ACTIVE) {
					state.state = UIInteractionState::ACTIVE;
					if (entity.HasComponent<MouseOffsetComponent>()) {
						entity.GetComponent<MouseOffsetComponent>().offset = mouse_position - transform.position;
					}
					if (entity.HasComponent<ActiveColorComponent>()) {
						background.color = entity.GetComponent<ActiveColorComponent>().color;
					}
					if (entity.HasComponent<EventComponent>()) {
						engine::EventHandler::Invoke(entity, entity);
					}
				}
			} else {
				state.state = UIInteractionState::NONE;
				background.color = background.original_color;
			}
		}
	}
};

class UIButtonRenderer : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, StateComponent, RenderComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, transform, size, background, state, render] : entities) {
			engine::TextureManager::DrawSolidRectangle(transform.position, size.size, background.color);
			if (entity.HasComponent<TextComponent>()) {
				auto& text = entity.GetComponent<TextComponent>();
				engine::FontManager::Draw(text.content, transform.position, size.size);
			}
		}
	}
};

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