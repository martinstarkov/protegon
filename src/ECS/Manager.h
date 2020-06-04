#pragma once

#include <iostream>
#include <algorithm>
#include <bitset>
#include <array>
#include <limits>
#include <cstddef>
#include <map>
#include <vector>
#include <utility>
#include <memory>
#include <tuple>

#include "Types.h"
#include "../Vec2D.h"
#include "Systems.h"

template<typename K, typename V>
static void print_map(std::map<K, V> const& m) {
	for (auto const& pair : m) {
		std::cout << "{" << pair.first << ": " << pair.second << "}\n";
	}
}

static EntityID getNewEntityID() {
	static EntityID lastID = 0U;
	return lastID++;
}

class Entity;
class BaseSystem;

class Manager {
public:
	Manager(const Manager&) = delete;
	Manager& operator=(const Manager&) = default; // was delete
	Manager(Manager&&) = delete;
	Manager& operator=(Manager&&) = default; // was delete
	Manager() {}
	~Manager() {}
	bool init() {
		createSystems();
		return true;
	}
	Entity* createEntity();
	Entity* createTree(float x, float y);
	Entity* createBox(float x, float y);
	Entity* createGhost(float x, float y, float lifetime = 5.0f);
	void createSystems();
	void updateSystems();
	void refreshSystems() {
		for (auto& epair : _entities) {
			for (auto& spair : _systems) {
				spair.second->onEntityChanged(epair.second.get());
			}
		}
	}
	void refreshSystems(Entity* entity) {
		for (auto& spair : _systems) {
			spair.second->onEntityChanged(entity);
		}
	}
	void refresh() {
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
	template <typename TSystem> void createSystem() {
		std::unique_ptr<TSystem> system = std::make_unique<TSystem>();
		SystemID ID = typeid(TSystem).hash_code();
		if (_systems.find(ID) == _systems.end()) {
			_systems.emplace(ID, std::move(system));
		}
	}
	template <typename TSystem> TSystem* getSystem() {
		auto it = _systems.find(typeid(TSystem).hash_code());
		if (it != _systems.end()) {
			return static_cast<TSystem*>(it->second.get());
		}
		return nullptr;
	}
private:
	using SystemID = std::size_t;
	std::map<EntityID, std::unique_ptr<Entity>> _entities;
	std::map<SystemID, std::unique_ptr<BaseSystem>> _systems;
	void destroyEntity(EntityID entityID) {
		auto it = _entities.find(entityID);
		if (it != _entities.end()) {
			for (auto& pair : _systems) {
				pair.second->onEntityDestroyed(entityID);
			}
			_entities.erase(entityID);
		}
	}
};