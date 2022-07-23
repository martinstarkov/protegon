//#pragma once
//
//#include "ecs/System.h"
//
//#include "renderer/TextureManager.h"
//
//#include "core/Scene.h"
//
//#include "physics/collision/CollisionFunctions.h"
//
//class RenderSystem : public ecs::System<RenderComponent, TransformComponent, SpriteComponent> {
//public:
//	virtual void Update() override final {
//		int counter{ 0 };
//		auto& scene{ engine::Scene::Get() };
//		for (auto& [entity, render, transform, sprite] : entities) {
//			V2_double position{ transform.position };
//			V2_double size{sprite.current_sprite.size };
//			// TODO: Cull objects out of view.
//			V2_double hitbox_offset;
//			if (entity.HasComponent<AnimationComponent>()) {
//				auto& animation{ entity.GetComponent<AnimationComponent>() };
//				hitbox_offset = sprite.sprite_map.GetAnimation(animation.current_animation).hitbox_offset;
//			}
//			assert(!size.IsZero() && "Cannot render sprite without (collision or size) component");
//			auto flip = Flip::NONE;
//			V2_double flip_scaling{};
//			if (entity.HasComponent<DirectionComponent>()) {
//				auto& dir{ entity.GetComponent<DirectionComponent>() };
//				if (dir.x_direction == Direction::LEFT) {
//					flip = Flip::HORIZONTAL;
//					if (static_cast<int>(size.x) % 2 == 1) {
//						flip_scaling.x = 0;
//					} else {
//						flip_scaling.x = 1;
//					}
//					if (dir.y_direction == Direction::UP) {
//						flip_scaling.y = hitbox_offset.y;
//						flip = Flip::BOTH;
//					} else {
//						flip_scaling.y = 0;
//					}
//				} else if (dir.y_direction == Direction::UP) {
//					flip_scaling.y = hitbox_offset.y;
//					flip = Flip::VERTICAL;
//				} else {
//					flip_scaling.y = 0;
//				}
//			}
//			auto sprite_position{ position - (hitbox_offset - flip_scaling) * sprite.scale };
//			++counter;
//			engine::TextureManager::DrawRectangle(
//				sprite.sprite_map.path,
//				sprite.current_sprite.position,
//				sprite.current_sprite.size,
//				scene.WorldToScreen(sprite_position),
//				scene.Scale(size * sprite.scale),
//				flip,
//				transform.center_of_rotation,
//				transform.rotation);
//		}
//		//LOG("Rendered " << counter << " sprites");
//	}
//};