#include "Manager.h"
#include "Components.h"
#include "Systems.h"
#include "Entity.h"

Entity* Manager::createEntity() {
	EntityID id = getNewEntityID();
	std::unique_ptr<Entity> entity = std::make_unique<Entity>(id, this);
	Entity* temp = entity.get();
	_entities.emplace(id, std::move(entity));
	return temp;
}

Entity* Manager::createTree(float x, float y) {
	Entity* entity = createEntity();
	entity->addComponent<TransformComponent>(Vec2D(x, y));
	entity->addComponent<SizeComponent>(Vec2D(32, 32));
	entity->addComponent<SpriteComponent>("./resources/textures/enemy.png", AABB(0, 0, 16, 16));
	entity->addComponent<RenderComponent>();
	return entity;
}
Entity* Manager::createBox(float x, float y) {
	Entity* entity = createEntity();
	entity->addComponent<TransformComponent>(Vec2D(x, y));
	entity->addComponent<SizeComponent>(Vec2D(16, 16));
	entity->addComponent<MotionComponent>(Vec2D(0.1f, 0.1f));
	entity->addComponent<GravityComponent>();
	entity->addComponent<SpriteComponent>("./resources/textures/player.png", AABB(0, 0, 16, 16));
	entity->addComponent<RenderComponent>();
	return entity;
}

Entity* Manager::createGhost(float x, float y, float lifetime) {
	Entity* entity = createEntity();
	entity->addComponent<TransformComponent>(Vec2D(x, y));
	entity->addComponent<SizeComponent>(Vec2D(16, 16));
	entity->addComponent<MotionComponent>(Vec2D(0.01f, 0.0f));
	entity->addComponent<GravityComponent>();
	entity->addComponent<LifetimeComponent>(lifetime);
	entity->addComponent<RenderComponent>();
	return entity;
}

void Manager::createSystems() {
	createSystem<RenderSystem>();
	createSystem<MovementSystem>();
	createSystem<GravitySystem>();
	createSystem<LifetimeSystem>();
}
void Manager::updateSystems() {
	getSystem<GravitySystem>()->update();
	getSystem<MovementSystem>()->update();
	getSystem<LifetimeSystem>()->update();
}