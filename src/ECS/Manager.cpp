
#include "Manager.h"

Entity* Manager::getEntity(EntityID entityID) {
	auto iterator = _entities.find(entityID);
	if (iterator != _entities.end()) {
		return iterator->second.get();
	}
	std::cout << "Entity (" << entityID << ") does not exist in Manager (" << this << ")" << std::endl;
	return nullptr;
}

Entity* Manager::createEntity() {
	EntityID id = getNewEntityID();
	std::unique_ptr<Entity> entity = std::make_unique<Entity>(id, this);
	Entity* temp = entity.get();
	_entities.emplace(id, std::move(entity));
	return temp;
}

Entity* Manager::createTree(float x, float y) {
	Entity* entity = createEntity();
	LOG_("Tree created : ");
	AllocationMetrics::printMemoryUsage();
	entity->addComponent<TransformComponent>(Vec2D(x, y));
	LOG_("(" << sizeof(TransformComponent));
	LOG_(") TransformComponent added : ");
	AllocationMetrics::printMemoryUsage();
	entity->addComponent<SizeComponent>(Vec2D(32, 32));
	LOG_("(" << sizeof(SizeComponent));
	LOG_(") SizeComponent added : ");
	AllocationMetrics::printMemoryUsage();
	entity->addComponent<SpriteComponent>("./resources/textures/enemy.png", AABB(0, 0, 16, 16));
	LOG_("(" << sizeof(SpriteComponent));
	LOG_(") SpriteComponent added : ");
	AllocationMetrics::printMemoryUsage();
	entity->addComponent<RenderComponent>();
	LOG_("(" << sizeof(RenderComponent));
	LOG_(") RenderComponent added : ");
	AllocationMetrics::printMemoryUsage();
	LOG_("Tree components added : ");
	AllocationMetrics::printMemoryUsage();
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
	createSystem<RenderSystem>(); // 124 bytes
	createSystem<MovementSystem>(); // 120 bytes
	createSystem<GravitySystem>(); // 120 bytes
	createSystem<LifetimeSystem>(); // 116 bytes
}

void Manager::updateSystems() {
	assert(getSystem<GravitySystem>().lock() != nullptr);
	assert(getSystem<MovementSystem>().lock() != nullptr);
	assert(getSystem<LifetimeSystem>().lock() != nullptr);

	getSystem<GravitySystem>().lock()->update();
	getSystem<MovementSystem>().lock()->update();
	getSystem<LifetimeSystem>().lock()->update();
}