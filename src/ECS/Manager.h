#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <bitset>
#include <array>
#include <map>

#include "Entity.h"

#include "Systems.h"

class Component;
class BaseSystem;

static EntityID getNewEntityID() {
	static EntityID lastID = 0U;
	return lastID++;
}

class Manager {
private:
	using SystemID = std::size_t;
	//using Entities = ;
	using Components = std::vector<std::vector<Component*>>;
	using Systems = std::map<SystemID, BaseSystem*>;
	std::map<EntityID, Entity*> entities;
	Components components;
	Systems systems;
public:
	Manager(const Manager&) = delete;
	Manager& operator=(const Manager&) = delete;
	Manager(Manager&&) = delete;
	Manager& operator=(Manager&&) = delete;
	Manager() {}
	~Manager() {}
	bool init() {
		createSystems();
		for (auto& epair : entities) {
			for (auto& spair : systems) {
				spair.second->onEntityCreated(epair.second);
			}
		}
		return true;
	}
	void update() {
	}
	Entity& createEntity() {
		EntityID newID = getNewEntityID();
		std::cout << "Created entity: " << newID << std::endl;
		Entity* entity = new Entity(newID);
		entities.insert({ newID, entity });
		return *entity;
	}
	Entity& createTree(float x, float y) {
		Entity& entity = createEntity();
		entity.addComponent(new TransformComponent(entity.getID(), Vec2D(x, y)));
		entity.addComponent(new SizeComponent(entity.getID(), Vec2D(32, 32)));
		entity.addComponent(new SpriteComponent(entity.getID(), "./resources/textures/enemy.png", AABB(0, 0, 16, 16)));
		for (auto& pair : systems) {
			pair.second->onEntityCreated(&entity);
		}
		return entity;
	}
	Entity& createBox(float x, float y) {
		Entity& entity = createEntity();
		entity.addComponent(new TransformComponent(entity.getID(), Vec2D(x, y)));
		entity.addComponent(new SizeComponent(entity.getID(), Vec2D(16, 16)));
		entity.addComponent(new MotionComponent(entity.getID(), Vec2D(0.1f, 0.1f)));
		entity.addComponent(new SpriteComponent(entity.getID(), "./resources/textures/player.png", AABB(0, 0, 16, 16)));
		for (auto& pair : systems) {
			pair.second->onEntityCreated(&entity);
		}
		return entity;
	}
	Entity& createGhost(float x, float y) {
		Entity& entity = createEntity();
		entity.addComponent(new TransformComponent(entity.getID(), Vec2D(x, y)));
		entity.addComponent(new SizeComponent(entity.getID(), Vec2D(16, 16)));
		entity.addComponent(new MotionComponent(entity.getID(), Vec2D(0.2f, 0.2f)));
		for (auto& pair : systems) {
			pair.second->onEntityCreated(&entity);
		}
		return entity;
	}
	void destroyEntity(EntityID entityID) {
		auto it = entities.find(entityID);
		if (it != entities.end()) {
			std::cout << "Deleting: " << it->first << "," << it->second << std::endl;
			for (auto& pair : systems) {
				pair.second->onEntityDestroyed(entityID);
			}
			entities.erase(entityID);
			//delete it->second;
		}
	}
	void createSystems() {
		RenderSystem* renderSystem = new RenderSystem(this);
		SystemID ID1 = typeid(RenderSystem).hash_code();
		if (systems.find(ID1) == systems.end()) {
			systems.emplace(ID1, renderSystem);
		}
		MovementSystem* movementSystem = new MovementSystem(this);
		SystemID ID2 = typeid(MovementSystem).hash_code();
		if (systems.find(ID2) == systems.end()) {
			systems.emplace(ID2, movementSystem);
		}
	}
	template <typename TSystem> TSystem* getSystem() {
		SystemID ID = typeid(TSystem).hash_code();
		auto it = systems.find(ID);
		if (it != systems.end()) {
			return static_cast<TSystem*>(it->second);
		}
		return nullptr;
	}
};