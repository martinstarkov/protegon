#include "Manager.h"

#include "Entity.h"
#include "Systems.h"
#include "Components.h"
#include "../StateMachine/StateMachines.h"
#include "../StateMachine/States.h"

EntityData::EntityData(const EntityData& copy) {
	*this = copy;
}

EntityData& EntityData::operator=(EntityData const& other) {
	assert(&other && "Cannot copy deleted EntityData");
	alive = other.alive;
	for (auto& pair : other.components) {
		components.emplace(pair.first, pair.second->uniqueClone());
	}
	return *this;
}

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

EntitySet Manager::getEntities(EntitySet exclude) {
	EntitySet set;
	for (auto& pair : _entities) {
		if (exclude.find(pair.first) == exclude.end()) {
			set.insert(pair.first);
		}
	}
	return set;
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0;
	return lastID++;
}

EntityID Manager::createEntity() {
	EntityID id = getNewEntityID();
	_entities.emplace(id, std::make_unique<EntityData>());
	return id;
}

void Manager::destroyEntity(EntityID id) {
	auto it = _entities.find(id);
	if (it != _entities.end()) {
		it->second->alive = false;
	}
	// refresh systems later
}

EntityData* Manager::getEntityData(EntityID id) {
	auto it = _entities.find(id);
	if (it != _entities.end()) {
		return it->second.get();
	}
	return nullptr;
}

void Manager::copyEntityData(EntityID to, EntityID from) {
	// replace if statement with assert later
	if (hasEntity(to) && hasEntity(from)) {
		*getEntityData(to) = *getEntityData(from);
	} else {
		LOG("Cannot copy EntityData from " << from << " to " << to << " as both entities do not exist");
	}
}

bool Manager::hasEntity(EntityID id) {
	return _entities.find(id) != _entities.end();
}

EntityID Manager::createTree(Vec2D position) {
	EntityID id = createEntity();
	Entity handle = Entity(id, this);
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
	static bool init = true;
	static EntityID jId;
	static Entity jsonHandle;
	if (init) {
		init = false;
		jId = createEntity();
		jsonHandle = Entity(jId, this);
		std::ifstream in("resources/box.json");
		json j;
		in >> j;
		Serializer::deserialize(j["box"], jsonHandle);
		in.close();
	}
	if (hasEntity(jId)) {
		EntityID id = createEntity();
		copyEntityData(id, jId);
		addComponents(id, TransformComponent(position));
		refresh();
		return id;
	} else {
		init = true;
		LOG("Could not create Box Entity using deserialization");
		return UINT_MAX;
	}
}
EntityID Manager::createPlayer(Vec2D position) {
	EntityID id = createEntity();
	Entity handle = Entity(id, this);
	handle.addComponents(
		MotionComponent(),
		DragComponent(UNIVERSAL_DRAG),
		InputComponent(),
		PlayerController(Vec2D(1.0)),
		AnimationComponent(),
		StateMachineComponent({ {"walkStateMachine", new WalkStateMachine("idle")},
							  {"jumpStateMachine", new JumpStateMachine("grounded")} }),
		TransformComponent(position),
		SizeComponent(Vec2D(30, 51)),
		SpriteComponent("./resources/textures/player_test2.png", Vec2D(30, 51)),
		SpriteSheetComponent(),
		DirectionComponent(),
		RenderComponent(), 
		CollisionComponent()
	);
	return id;
}

void Manager::update() {
	getSystem<InputSystem>()->update();
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
	getSystem<AnimationSystem>()->update();
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

void Manager::setComponentHandle(BaseComponent* component, EntityID id) {
	component->setHandle(Entity(id, this));
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

void Manager::refreshDeleted() {
	// TODO: Somehow free up empty EntityIDs 
	// Possible solution: upgrade EntityID to unsigned long long it and change all function EntityID parameters to take refernces instead of copying
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