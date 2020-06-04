#pragma once

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

#include "Entity.h"
#include "Systems/BaseSystem.h"
#include "Systems.h"
#include "Components.h"

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
	Entity* getEntity(EntityID entityID);
	Entity* createEntity();
	Entity* createTree(float x, float y);
	Entity* createBox(float x, float y);
	Entity* createGhost(float x, float y, float lifetime = 2.0f);
	void createSystems();
	void updateSystems();
	void refreshSystems(Entity* entity) {
		for (auto& spair : _systems) {
			spair.second->onEntityChanged(entity);
		}
	}
	void refreshSystems() {
		for (auto& epair : _entities) {
			refreshSystems(epair.second.get());
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
		SystemID ID = typeid(TSystem).hash_code();
		if (_systems.find(ID) == _systems.end()) {
			std::shared_ptr<TSystem> system = std::make_shared<TSystem>();
			system->setManager(this);
			_systems.emplace(ID, std::move(system));
		} else {
			std::cout << "System with hash code (" << ID << ") already exists in Manager (" << this << ")" << std::endl;
		}
	}
	template <typename TSystem> std::weak_ptr<TSystem> getSystem() {
		auto iterator = _systems.find(typeid(TSystem).hash_code());
		if (iterator != _systems.end()) {
			return std::static_pointer_cast<TSystem>(iterator->second);
		}
		return std::weak_ptr<TSystem>();
	}
private:
	using SystemID = std::size_t;
	std::map<EntityID, std::unique_ptr<Entity>> _entities;
	std::map<SystemID, std::shared_ptr<BaseSystem>> _systems;
	void destroyEntity(EntityID entityID) {
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
};