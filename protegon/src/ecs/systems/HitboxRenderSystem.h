//#pragma once
//
//#include "ecs/System.h"
//
//#include "renderer/TextureManager.h"
//
//#include "core/Scene.h"
//
//class TileRenderSystem : public ecs::System<RenderComponent, TransformComponent, CollisionComponent> {
//public:
//	virtual void Update() override final {
//		auto& scene = engine::Scene::Get();
//		for (auto& [entity, render, transform, collider] : entities) {
//			engine::TextureManager::DrawSolidRectangle(
//				scene.WorldToScreen(transform.position),
//				scene.Scale(collider.collider.size),
//				render.color);
//		}
//	}
//private:
//};
//
//class HitboxRenderSystem : public ecs::System<RenderComponent, TransformComponent> {
//public:
//	virtual void Update() override final {
//		auto& scene = engine::Scene::Get();
//		for (auto& [entity, render, transform] : entities) {
//			V2_double position = transform.position;
//			V2_double size;
//			if (entity.HasComponent<CollisionComponent>()) {
//				size = entity.GetComponent<CollisionComponent>().collider.size;
//			} else if (entity.HasComponent<SizeComponent>()) {
//				size = entity.GetComponent<SizeComponent>().size;
//			}
//			// TEMPORARY: Exclude player hitbox.
//			if (!entity.HasComponent<PlayerController>()) {
//				engine::TextureManager::DrawSolidRectangle(
//					scene.WorldToScreen(position),
//					scene.Scale(size),
//					render.color);
//			}
//		}
//		for (auto [aabb, color] : DebugDisplay::rectangles()) {
//			engine::TextureManager::DrawRectangle(
//				scene.WorldToScreen(aabb.position),
//				scene.Scale(aabb.size),
//				color);
//		}
//		DebugDisplay::rectangles().clear();
//		for (auto [origin, destination, color] : DebugDisplay::lines()) {
//			engine::TextureManager::DrawLine(
//				scene.WorldToScreen(origin),
//				scene.Scale(destination),
//				color);
//		}
//		DebugDisplay::lines().clear();
//		for (auto [center, radius, color] : DebugDisplay::circles()) {
//			engine::TextureManager::DrawCircle(
//				scene.WorldToScreen(center),
//				scene.ScaleX(radius),
//				color);
//		}
//		DebugDisplay::circles().clear();
//	}
//};