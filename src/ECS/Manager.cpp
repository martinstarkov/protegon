
#include "Manager.h"

bool Manager::init() {
	create<SystemFactory>(RenderSystem(), MovementSystem(), GravitySystem(), LifetimeSystem());
	return true;
}

Entity* Manager::getEntity(EntityID entityID) {
	auto iterator = _entities.find(entityID);
	if (iterator != _entities.end()) {
		return iterator->second.get();
	}
	std::cout << "Entity (" << entityID << ") does not exist in Manager (" << this << ")" << std::endl;
	return nullptr;
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0U;
	return lastID++;
}

Entity* Manager::createEntity() {
	EntityID id = getNewEntityID();
	std::unique_ptr<Entity> entity = std::make_unique<Entity>(id, this);
	Entity* temp = entity.get();
	_entities.emplace(id, std::move(entity));
	return temp;
}

void Manager::destroyEntity(EntityID entityID) {
	auto it = _entities.find(entityID);
	if (it != _entities.end()) {
		for (auto& pair : _systems) {
			pair.second->onEntityDestroyed(entityID);
		}
		_entities.erase(entityID);
	} else {
		std::cout << "Entity (" << entityID << ") cannot be destroyed as it is not found in Manager (" << this << ")" << std::endl;
	}
}

Entity* Manager::createTree(float x, float y) {
	Entity* entity = create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(32, 32)), SpriteComponent("./resources/textures/enemy.png", AABB(0, 0, 16, 16)), RenderComponent());
	return entity;
}
Entity* Manager::createBox(float x, float y) {
	Entity* entity = create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(16, 16)), MotionComponent(Vec2D(0.1f, 0.1f)), GravityComponent(), SpriteComponent("./resources/textures/player.png", AABB(0, 0, 16, 16)), RenderComponent());
	return entity;
}
Entity* Manager::createGhost(float x, float y, float lifetime) {
	Entity* entity = create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(16, 16)), MotionComponent(Vec2D(0.01f, 0.0f)), GravityComponent(), LifetimeComponent(lifetime), RenderComponent());
	return entity;
}

void Manager::updateSystems() {
	assert(getSystem<GravitySystem>() != nullptr);
	assert(getSystem<MovementSystem>() != nullptr);
	assert(getSystem<LifetimeSystem>() != nullptr);

	getSystem<GravitySystem>()->update();
	getSystem<MovementSystem>()->update();
	getSystem<LifetimeSystem>()->update();
	refresh();
}

void Manager::refreshSystems(Entity* entity) {
	for (auto& spair : _systems) {
		spair.second->onEntityChanged(entity);
	}
}

void Manager::refreshSystems() {
	for (auto& epair : _entities) {
		refreshSystems(epair.second.get());
	}
}

void Manager::refresh() {
	std::vector<Entity*> deletables;
	for (auto& pair : _entities) {
		if (!pair.second->isAlive()) {
			deletables.emplace_back(pair.second.get());
		}
	}
	for (Entity* entity : deletables) {
		destroyEntity(entity->getID());
	}
	deletables.clear();
}