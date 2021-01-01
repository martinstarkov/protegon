#pragma once

#include "System.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"

#include "renderer/TextureManager.h"
#include "renderer/FontManager.h"
#include "renderer/AABB.h"

#include "physics/collision/static/PointvsAABB.h"

#include "utils/Vector2.h"

#include "ui/UIComponents.h"

class UIButtonListener : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, StateComponent> {
public:
	UIButtonListener() = default;
	UIButtonListener(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto [entity, transform, size, background, state] : entities) {
			if (entity.HasComponent<EventComponent>() &&
				engine::InputHandler::KeyDown(Key::SPACEBAR) &&
				entity.HasComponent<TextComponent>() &&
				entity.GetComponent<TextComponent>().content == "Play") {
				assert(scene != nullptr && "Scene not given to UIButtonListener");
				engine::EventHandler::Invoke(entity, entity, *scene);
			}
			auto surface = AABB{ transform.position, size.size };
			auto mouse_position = engine::InputHandler::GetMousePosition();
			bool hovering = engine::collision::PointvsAABB(mouse_position, surface);
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
						entity.GetComponent<MouseOffsetComponent>().offset = static_cast<V2_double>(mouse_position) - transform.position;
					}
					if (entity.HasComponent<ActiveColorComponent>()) {
						background.color = entity.GetComponent<ActiveColorComponent>().color;
					}
					if (entity.HasComponent<EventComponent>()) {
						assert(scene != nullptr && "Scene not given to UIButtonListener");
						engine::EventHandler::Invoke(entity, entity, *scene);
					}
				}
			} else {
				state.state = UIInteractionState::NONE;
				background.color = background.original_color;
			}
		}
	}
private:
	engine::Scene* scene = nullptr;
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