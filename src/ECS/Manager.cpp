#include "Manager.h"

#include "EntityHandle.h"
#include "Systems.h"
#include "Components.h"
#include "../StateMachine/StateMachines.h"
#include "../StateMachine/States.h"

void Manager::init() {
	createSystems(
		RenderSystem(),
		MovementSystem(),
		GravitySystem(),
		LifetimeSystem(),
		AnimationSystem(),
		CollisionSystem(),
		MotionSystem(),
		InputSystem(),
		DragSystem(),
		StateMachineSystem(),
		DirectionSystem()
	);
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
	handle.addComponents(
		TransformComponent(position), 
		SizeComponent(Vec2D(64)), 
		SpriteComponent("./resources/textures/tree.png", Vec2D(16)), 
		RenderComponent(), 
		CollisionComponent()
	);
	return id;
}
EntityID Manager::createBox(Vec2D position) {
	EntityID id = createEntity();
	EntityHandle handle = EntityHandle(id, this);
	handle.addComponents(
		TransformComponent(position), 
		SizeComponent(Vec2D(32)), 
		SpriteComponent("./resources/textures/box.png", Vec2D(16)), 
		MotionComponent(Vec2D(0.5), {}, Vec2D(3.0)), 
		DragComponent(UNIVERSAL_DRAG), 
		RenderComponent(), 
		LifetimeComponent(5.0),
		CollisionComponent()
	);
	return id;
}
EntityID Manager::createPlayer(Vec2D position) {
	EntityID id = createEntity();
	EntityHandle handle = EntityHandle(id, this);
	handle.addComponents(
		MotionComponent(),
		DragComponent(UNIVERSAL_DRAG),
		InputComponent(),
		PlayerController(Vec2D(1.0, 1.0)),
		StateMachineComponent({ {"walkStateMachine", new WalkStateMachine("idle", handle)},{"jumpStateMachine", new JumpStateMachine("grounded", handle)} }),
		TransformComponent(position),
		SizeComponent(Vec2D(50, 50)), 
		SpriteComponent("./resources/textures/player_anim.png", Vec2D(16)), 
		AnimationComponent(8), 
		RenderComponent(), 
		CollisionComponent(),
		DirectionComponent()
	);
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
	getSystem<DirectionSystem>()->update();
	getSystem<StateMachineSystem>()->update();
	refreshDeleted();
}

void Manager::render() {
	getSystem<RenderSystem>()->update();
}

bool Manager::hasComponent(EntityID id, ComponentID cId) {
	auto it = _entities.find(id);
	if (it != _entities.end()) {
		ComponentMap& components = it->second->components;
		auto cIt = components.find(cId);
		if (cIt != components.end()) {
			return true;
		}
	}
	return false;
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