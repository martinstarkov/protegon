#include "RenderSystem.h"

#include "../Components/TransformComponent.h"
#include "../Components/SizeComponent.h"
#include "../Components/SpriteComponent.h"
#include "../Components/DirectionComponent.h"

#include "../../TextureManager.h"

void RenderSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		TransformComponent* transform = e.getComponent<TransformComponent>();
		SizeComponent* size = e.getComponent<SizeComponent>();
		SpriteComponent* sprite = e.getComponent<SpriteComponent>();
		DirectionComponent* direction = e.getComponent<DirectionComponent>();
		if (sprite) {
			SDL_RendererFlip flip = SDL_FLIP_NONE;
			if (direction) {
				flip = static_cast<SDL_RendererFlip>(direction->xDirection | direction->yDirection);
			}
			TextureManager::draw(sprite->texture, sprite->source, AABB(transform->position, size->size).AABBtoRect(), 0.0, flip);
		} else {
			TextureManager::draw(AABB(transform->position, size->size).AABBtoRect());
		}
	}
}
