#include "Manager.h"

bool Manager::init() {
	create<SystemFactory>(RenderSystem(), MovementSystem(), GravitySystem(), LifetimeSystem(), AnimationSystem(), CollisionSystem(), MotionSystem(), InputSystem(), DragSystem(), StateMachineSystem());
	return true;
}

Entity& Manager::getEntity(EntityID entityID) {
	auto iterator = _entities.find(entityID);
	assert(iterator != _entities.end() && "Must return valid reference to entityID");
	return *iterator->second; // reference to entity unique ptr
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0;
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
	auto iterator = _entities.find(entityID);
	assert(iterator != _entities.end() && "Attempting to destroy non-existent entity");
	for (auto& pair : _systems) {
		pair.second->onEntityDestroyed(entityID);
	}
	_entities.erase(entityID);
}

EntityID Manager::createTree(Vec2D position) {
	EntityID entityID = create<EntityFactory>(TransformComponent(position), SizeComponent(Vec2D(64)), SpriteComponent("./resources/textures/tree.png", Vec2D(16)), RenderComponent(), CollisionComponent());
	return entityID;
}
EntityID Manager::createBox(Vec2D position) {
	EntityID entityID = create<EntityFactory>(TransformComponent(position), SizeComponent(Vec2D(32)), SpriteComponent("./resources/textures/box.png", Vec2D(16)), MotionComponent(Vec2D(0.5), {}, Vec2D(3.0)), DragComponent(UNIVERSAL_DRAG), RenderComponent(), CollisionComponent());
	return entityID;
}
EntityID Manager::createPlayer(Vec2D position) {
	EntityID entityID = create<EntityFactory>(TransformComponent(position), SizeComponent(Vec2D(50, 50)), SpriteComponent("./resources/textures/player_anim.png", Vec2D(16)), AnimationComponent(8), RenderComponent(), CollisionComponent());
	Entity& entity = getEntity(entityID);
	StateMachineComponent sm;
	std::unique_ptr<WalkStateMachine> wsm = std::make_unique<WalkStateMachine>();
	wsm->init("idle", &entity);

	std::unique_ptr<JumpStateMachine> jsm = std::make_unique<JumpStateMachine>();
	jsm->init("grounded", &entity);

	sm.stateMachines.emplace("walkStateMachine", std::move(wsm));
	sm.stateMachines.emplace("jumpStateMachine", std::move(jsm));
	sm.setNames();
	entity.addComponents(MotionComponent(), DragComponent(UNIVERSAL_DRAG), InputComponent(), PlayerController(Vec2D(1.0, 1.0)), std::move(sm));
	entity.addComponents();
	return entityID;
}

void Manager::updateSystems() {
	getSystem<InputSystem>()->update();
	getSystem<AnimationSystem>()->update();
	getSystem<GravitySystem>()->update();
	getSystem<MotionSystem>()->update();
	getSystem<DragSystem>()->update();
	getSystem<MovementSystem>()->update();
	getSystem<CollisionSystem>()->update();
	getSystem<LifetimeSystem>()->update();
	getSystem<StateMachineSystem>()->update();
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