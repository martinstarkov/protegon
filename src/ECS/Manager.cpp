
#include "Manager.h"

bool Manager::init() {
	create<SystemFactory>(RenderSystem(), MovementSystem(), GravitySystem(), LifetimeSystem(), AnimationSystem());
	return true;
}

Entity& Manager::getEntity(EntityID entityID) {
	auto iterator = _entities.find(entityID);
	assert(iterator != _entities.end() && "Must return valid reference to entityID");
	return *iterator->second; // reference to entity unique ptr
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0U;
	return lastID++;
}

Entity& Manager::createEntity() {
	EntityID id = getNewEntityID();
	std::unique_ptr<Entity> uPtr = std::make_unique<Entity>(id, this);
	Entity& ref = *uPtr;
	_entities.emplace(id, std::move(uPtr));
	return ref;
}

void Manager::destroyEntity(EntityID entityID) {
	auto it = _entities.find(entityID);
	if (it != _entities.end()) {
		for (auto& pair : _systems) {
			pair.second->onEntityDestroyed(entityID);
		}
		_entities.erase(entityID);
	} else {
		LOG("Entity (" << entityID << ") cannot be destroyed as it is not found in Manager (" << this << ")");
	}
}

EntityID Manager::createTree(float x, float y) {
	return create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(32, 32)), SpriteComponent("./resources/textures/enemy.png", Vec2D(16, 16)), RenderComponent());
}
EntityID Manager::createBox(float x, float y) {
	return create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(50, 50)), SpriteComponent("./resources/textures/player_anim.png", Vec2D(16, 16), 8), AnimationComponent(0.08f), RenderComponent());
}
EntityID Manager::createGhost(float x, float y, float lifetime) {
	return create<EntityFactory>(TransformComponent(Vec2D(x, y)), SizeComponent(Vec2D(16, 16)), MotionComponent(Vec2D(0.1f, 0.0f)), RenderComponent());
}

void Manager::updateSystems() {
	assert(getSystem<GravitySystem>() != nullptr);
	assert(getSystem<MovementSystem>() != nullptr);
	assert(getSystem<LifetimeSystem>() != nullptr);

	getSystem<AnimationSystem>()->update();
	getSystem<GravitySystem>()->update();
	getSystem<MovementSystem>()->update();
	getSystem<LifetimeSystem>()->update();
	refresh();
}

void Manager::refreshSystems(const EntityID entityID) {
	for (auto& pair : _systems) {
		pair.second->onEntityChanged(entityID);
	}
}

void Manager::refreshSystems() {
	for (auto& pair : _entities) {
		refreshSystems(pair.first);
	}
}

void Manager::refresh() { // REWORK
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