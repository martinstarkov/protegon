#include "Manager.h"

#include "EntityHandle.h"
#include "Systems.h"
#include "Components.h"
#include "../StateMachine/StateMachines.h"
#include "../StateMachine/States.h"

void Manager::init() {
	createSystems(RenderSystem(), MovementSystem(), GravitySystem(), LifetimeSystem(), AnimationSystem(), CollisionSystem(), MotionSystem(), InputSystem(), DragSystem(), StateMachineSystem());
}

bool Manager::hasEntity(EntityID id) {
	return _entities.find(id) != _entities.end();
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0;
	return lastID++;
}

EntityID Manager::createEntity() {
	EntityID id = getNewEntityID();
	std::unique_ptr<Entity> uPtr = std::make_unique<Entity>();
	_entities.emplace(id, std::move(uPtr));
	return id;
}

void Manager::destroyEntity(EntityID id) {
	auto it = _entities.find(id);
	if (it != _entities.end()) {
		it->second->alive = false;
	}
	// refresh systems later
}

EntityID Manager::createTree(Vec2D position) {
	EntityID id = createEntity();
	EntityHandle handle = EntityHandle(id, this);
	handle.addComponents(TransformComponent(position), SizeComponent(Vec2D(64)), SpriteComponent("./resources/textures/tree.png", Vec2D(16)), RenderComponent(), CollisionComponent());
	return id;
}
EntityID Manager::createBox(Vec2D position) {
	EntityID id = createEntity();
	EntityHandle handle = EntityHandle(id, this);
	handle.addComponents(TransformComponent(position), SizeComponent(Vec2D(32)), SpriteComponent("./resources/textures/box.png", Vec2D(16)), MotionComponent(Vec2D(0.5), {}, Vec2D(3.0)), DragComponent(UNIVERSAL_DRAG), RenderComponent(), CollisionComponent());
	return id;
}
EntityID Manager::createPlayer(Vec2D position) {
	EntityID id = createEntity();
	EntityHandle handle = EntityHandle(id, this);
	handle.addComponents(TransformComponent(position), SizeComponent(Vec2D(50, 50)), SpriteComponent("./resources/textures/player_anim.png", Vec2D(16)), AnimationComponent(8), RenderComponent(), CollisionComponent());
	StateMachineComponent sm;
	std::unique_ptr<WalkStateMachine> wsm = std::make_unique<WalkStateMachine>();
	wsm->init("idle", handle);

	std::unique_ptr<JumpStateMachine> jsm = std::make_unique<JumpStateMachine>();
	jsm->init("grounded", handle);

	sm.stateMachines.emplace("walkStateMachine", std::move(wsm));
	sm.stateMachines.emplace("jumpStateMachine", std::move(jsm));
	sm.setNames();
	handle.addComponents(MotionComponent(), DragComponent(UNIVERSAL_DRAG), InputComponent(), PlayerController(Vec2D(1.0, 1.0)), std::move(sm));
	handle.addComponents();
	return id;
}

void Manager::update() {
	getSystem<InputSystem>()->update();
	getSystem<AnimationSystem>()->update();
	getSystem<GravitySystem>()->update();
	getSystem<MotionSystem>()->update();
	getSystem<DragSystem>()->update();
	getSystem<MovementSystem>()->update();
	getSystem<CollisionSystem>()->update();
	getSystem<LifetimeSystem>()->update();
	getSystem<StateMachineSystem>()->update();
	refreshDeleted();
}

void Manager::entityChanged(EntityID id) {
	for (auto& pair : _systems) {
		pair.second->onEntityChanged(id);
	}
}

void Manager::entityDestroyed(EntityID id) {
	for (auto& pair : _systems) {
		pair.second->onEntityDestroyed(id);
	}
}

void Manager::refresh() {
	for (auto& pair : _entities) {
		entityChanged(pair.first);
	}
}

void Manager::refreshDeleted() { // REWORK
	int iteratorOffset = 0;
	for (auto it = _entities.begin(); it != _entities.end(); ) {
		if (!it->second->alive) {
			entityDestroyed(it->first);
			_entities.erase(it++);
		} else {
			++it;
		}
	}
}