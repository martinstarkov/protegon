#pragma once

#include "System.h"

#include "renderer/TextureManager.h"

#include "core/Scene.h"

class RenderSystem : public ecs::System<RenderComponent, TransformComponent, SpriteComponent> {
public:
	RenderSystem() = default;
	RenderSystem(engine::Scene* scene) : scene{ scene } {}
	virtual void Update() override final {
		int counter = 0;
		if (scene) {
			auto camera = scene->GetCamera();
			for (auto& [entity, render, transform, sprite] : entities) {
				V2_double position = transform.position;
				V2_double size = sprite.current_sprite.size;
				if (camera) {
					AABB aabb{ position - camera->offset, size };
					AABB view{ { 0, 0 }, engine::Engine::ScreenSize() };
					// Cull objects outside of view.
					if (!engine::collision::AABBvsAABB(aabb, view)) {
						continue;
					}
				}
				V2_double hitbox_offset{};
				if (entity.HasComponent<AnimationComponent>()) {
					auto& animation = entity.GetComponent<AnimationComponent>();
					hitbox_offset = sprite.sprite_map.GetAnimation(animation.current_animation).hitbox_offset;
				}
				assert(!size.IsZero() && "Cannot render sprite without (collision or size) component");
				auto flip = Flip::NONE;
				V2_double flip_scaling{};
				if (entity.HasComponent<DirectionComponent>()) {
					auto& dir = entity.GetComponent<DirectionComponent>();
					if (dir.x_direction == Direction::LEFT) {
						flip = Flip::HORIZONTAL;
						if (static_cast<int>(size.x) % 2 == 1) {
							flip_scaling.x = 0;
						} else {
							flip_scaling.x = 1;
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
				auto sprite_position = position - (hitbox_offset - flip_scaling) * sprite.scale;
				if (camera) {
					sprite_position -= camera->offset;
					sprite_position *= camera->scale;
					size *= camera->scale;
					position -= camera->offset;
					position *= camera->scale;
				}
				++counter;
				engine::TextureManager::DrawRectangle(
					sprite.sprite_map.path,
					sprite.current_sprite.position,
					sprite.current_sprite.size,
					sprite_position,
					size * sprite.scale,
					flip,
					transform.center_of_rotation,
					transform.rotation);
			}
		}
		//LOG("Rendered " << counter << " sprites");
	}
private:
	engine::Scene* scene = nullptr;
};