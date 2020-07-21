#include "RenderSystem.h"

#include "../Components/TransformComponent.h"
#include "../Components/SizeComponent.h"
#include "../Components/SpriteComponent.h"

#include "../../TextureManager.h"

void RenderSystem::update() {
	for (auto& id : entities) {
		EntityHandle e = EntityHandle(id, manager);
		TransformComponent* transform = e.getComponent<TransformComponent>();
		SizeComponent* size = e.getComponent<SizeComponent>();
		SpriteComponent* sprite = e.getComponent<SpriteComponent>();
		if (sprite) {
			TextureManager::draw(sprite->texture, sprite->source, AABB(transform->position, size->size).AABBtoRect());
		} else {
			TextureManager::draw(AABB(transform->position, size->size).AABBtoRect());
		}
	}
}
