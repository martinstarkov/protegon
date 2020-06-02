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

template<typename K, typename V>
void print_map(std::map<K, V> const& m) {
	for (auto const& pair : m) {
		std::cout << "{" << pair.first << ": " << pair.second << "}\n";
	}
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0U;
	return lastID++;
}

class Manager {
private:
	using SystemID = std::size_t;
	using Systems = std::map<SystemID, BaseSystem*>;
	UniqueEntities entities;
	Systems systems;
	void destroyEntity(EntityID entityID) {
		auto it = entities.find(entityID);
		if (it != entities.end()) {
			for (auto& pair : systems) {
				pair.second->onEntityDestroyed(entityID);
			}
			entities.erase(entityID);
		}
	}
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
			refreshSystems(epair.second.get());
		}
		return true;
	}
	Entity* createEntity() {
		EntityID newID = getNewEntityID();
		std::unique_ptr<Entity> entity = std::make_unique<Entity>(newID);
		Entity* temp = entity.get();
		entities.emplace(newID, std::move(entity));
		return temp;
	}
	void createSystems() {
		createSystem<RenderSystem>();
		createSystem<MovementSystem>();
		createSystem<GravitySystem>();
		createSystem<LifetimeSystem>();
	}
	void updateSystems() {
		getSystem<GravitySystem>()->update();
		getSystem<MovementSystem>()->update();
		getSystem<LifetimeSystem>()->update();
	}
	void refreshSystems() {
		for (auto& epair : entities) {
			for (auto& spair : systems) {
				spair.second->onEntityCreated(epair.second.get());
			}
		}
	}
	void refreshSystems(Entity* entity) {
		for (auto& pair : systems) {
			pair.second->onEntityCreated(entity);
		}
	}
	void refresh() {
		std::vector<Entity*> deletables;
		for (auto& pair : entities) {
			if (!pair.second->isAlive()) {
				deletables.emplace_back(pair.second.get());
			}
		}
		for (Entity* entity : deletables) {
			destroyEntity(entity->getID());
		}
		deletables.clear();
	}
	template <typename TSystem> void createSystem() {
		TSystem* system = new TSystem();
		SystemID ID = typeid(TSystem).hash_code();
		if (systems.find(ID) == systems.end()) {
			systems.emplace(ID, system);
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