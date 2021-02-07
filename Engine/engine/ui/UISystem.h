#pragma once

#include "ecs/System.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"

#include "renderer/TextureManager.h"
#include "renderer/FontManager.h"
#include "renderer/AABB.h"

#include "physics/collision/static/PointvsAABB.h"

#include "math/Vector2.h"

#include "ui/UIComponents.h"
#include "ecs/components/TransformComponent.h"
#include "ecs/components/SizeComponent.h"
#include "ecs/components/RenderComponent.h"

class UIButtonListener : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, StateComponent, EventComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, transform, size, background, state, event] : entities) {
			auto surface = AABB{ transform.position, size.size };
			auto mouse_position = engine::InputHandler::GetMousePosition();
			bool hovering = engine::collision::PointvsAABB(mouse_position, surface);
			if (hovering) {
				if (engine::InputHandler::MouseReleased(MouseButton::LEFT)) {
					state.state = UIInteractionState::HOVER;
					if (entity.HasComponent<HoverColorComponent>()) {
						background.color = entity.GetComponent<HoverColorComponent>().color;
					} else {
						background.color = background.original_color;
					}
				} else if (engine::InputHandler::MousePressed(MouseButton::LEFT) && state.state != UIInteractionState::ACTIVE) {
					state.state = UIInteractionState::ACTIVE;
					if (entity.HasComponent<MouseOffsetComponent>()) {
						entity.GetComponent<MouseOffsetComponent>().offset = mouse_position - transform.position;
					}
					if (entity.HasComponent<ActiveColorComponent>()) {
						background.color = entity.GetComponent<ActiveColorComponent>().color;
					}
					engine::EventHandler::Invoke(entity);
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

class UITextRenderer : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, RenderComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, transform, size, background, render] : entities) {
			engine::TextureManager::DrawSolidRectangle(transform.position, size.size, background.color);
			if (entity.HasComponent<TextComponent>()) {
				auto& text = entity.GetComponent<TextComponent>();
				engine::FontManager::Draw(text.content, transform.position, size.size);
			}
		}
	}
};