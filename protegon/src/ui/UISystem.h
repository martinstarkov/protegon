//#pragma once
//
//#include "ecs/System.h"
//
//#include "event/InputHandler.h"
//#include "event/EventHandler.h"
//
//#include "renderer/TextureManager.h"
//#include "renderer/FontManager.h"
//#include "renderer/AABB.h"
//
//#include "physics/collision/static/PointvsAABB.h"
//
//#include "math/Vector2.h"
//
//#include "ui/UIComponents.h"
//#include "ecs/components/TransformComponent.h"
//#include "ecs/components/SizeComponent.h"
//#include "ecs/components/RenderComponent.h"
//
//class UIRenderer : public ecs::System<TransformComponent, SizeComponent, BackgroundColorComponent, RenderComponent> {
//public:
//	virtual void Update() override final {
//		for (auto [entity, transform, size, background, render] : entities) {
//			engine::TextureManager::DrawSolidRectangle(transform.position, size.size, background.color);
//			if (entity.HasComponent<TextComponent>()) {
//				auto& text{ entity.GetComponent<TextComponent>() };
//				engine::FontManager::Draw(text.content, transform.position, size.size);
//			}
//		}
//	}
//};