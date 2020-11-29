#pragma once

#include "System.h"

#include "renderer/TextureManager.h"

#include "core/Scene.h"

class RenderSystem : public ecs::System<RenderComponent, TransformComponent> {
public:
	RenderSystem() = default;
	RenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		for (auto& [entity, render, transform] : entities) {
			V2_double position = transform.position;
			V2_double size;
			bool has_sprite = entity.HasComponent<SpriteComponent>();
			if (entity.HasComponent<CollisionComponent>()) {
				size = entity.GetComponent<CollisionComponent>().collider.size;
			} else if (entity.HasComponent<SizeComponent>()) {
				size = entity.GetComponent<SizeComponent>().size;
			}
			if (has_sprite) {
				auto& sprite = entity.GetComponent<SpriteComponent>();
				assert(!size.IsZero() && "Cannot render sprite without (collision or size) component");
				auto flip = Flip::NONE;
				V2_double flip_scaling{};
				V2_double hitbox_offset{};
				if (entity.HasComponent<AnimationComponent>()) {
					auto& animation = entity.GetComponent<AnimationComponent>();
					hitbox_offset = sprite.sprite_map.GetAnimation(animation.current_animation).hitbox_offset;
				}
				if (entity.HasComponent<DirectionComponent>()) {
					auto& dir = entity.GetComponent<DirectionComponent>();
					if (dir.x_direction == Direction::LEFT) {
						flip = Flip::HORIZONTAL;
						if (static_cast<int>(size.x) % 2 == 1) {
							flip_scaling.x = 1;
						} else {
							flip_scaling.x = 0;
						}
						if (dir.y_direction == Direction::UP) {
							flip_scaling.y = hitbox_offset.y;
							flip = Flip::BOTH;
						} else {
							flip_scaling.y = 0;
						}
					} else if (dir.y_direction == Direction::UP) {
						flip_scaling.y = hitbox_offset.y;
						flip = Flip::VERTICAL;
					} else {
						flip_scaling.y = 0;
					}
				}
				size = sprite.current_sprite.size;
				if (scene) {
					auto camera = scene->GetCamera();
					auto texture_pos = position - (hitbox_offset - flip_scaling) * sprite.scale;
					if (camera) {
						// TRAIN OF THOUGHT: PERHAPS CENTERING OF CAMERA NOT WORKING DUE TO THIS POSITION NOT BEING ADDED TO THE TEXTURE DRAWING OR SOMETHING???
						position += camera->offset;
						position *= camera->scale;
						size *= camera->scale;
					}
					engine::TextureManager::DrawRectangle(
						sprite.sprite_map.path,
						sprite.current_sprite.position,
						sprite.current_sprite.size,
						texture_pos,
						size * sprite.scale,
						flip,
						transform.center_of_rotation,
						transform.rotation);
					engine::TextureManager::DrawRectangle(
						position,
						entity.GetComponent<CollisionComponent>().collider.size,
						render.color);
				}
			} else {
				if (scene) {
					auto camera = scene->GetCamera();
					if (camera) {
						position += camera->offset;
						position *= camera->scale;
						size *= camera->scale;
					}
					if (transform.rotation != 0) {
						engine::TextureManager::DrawRectangle(
							position,
							size,
							transform.rotation,
							transform.center_of_rotation,
							render.color);
					} else {
						engine::TextureManager::DrawRectangle(
							position,
							size,
							render.color);
					}
				}
			}
		}
		for (auto rectangle : DebugDisplay::rectangles()) {
			engine::TextureManager::DrawRectangle(
				rectangle.first.position,
				rectangle.first.size,
				rectangle.second);
		}
		DebugDisplay::rectangles().clear();
	}
private:
	engine::Scene* scene = nullptr;
};