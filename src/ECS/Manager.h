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
#include <type_traits>

#include "Types.h"
#include "../Vec2D.h"

#include "Entity.h"
#include "Systems/BaseSystem.h"
#include "Systems.h"
#include "Components.h"

class Manager {
public:
	Manager(const Manager&) = delete;
	Manager& operator=(const Manager&) = default; // was delete
	Manager(Manager&&) = delete;
	Manager& operator=(Manager&&) = default; // was delete
	Manager() {}
	~Manager() {}
	template<class TFunctor, typename... Ts> auto create(Ts&&... args) {
		return TFunctor::call(*this, std::forward<Ts>(args)...);
	}
	template <typename TSystem> TSystem* getSystem() {
		auto iterator = _systems.find(typeid(TSystem).hash_code());
		if (iterator != _systems.end()) {
			return static_cast<TSystem*>(iterator->second.get());
		}
		return nullptr;
	}
	template <typename TSystem> void createSystem(TSystem& system) {
		SystemID id = typeid(TSystem).hash_code();
		if (_systems.find(id) == _systems.end()) {
			std::unique_ptr<TSystem> uPtr = std::make_unique<TSystem>(std::move(system));
			const char* name = typeid(TSystem).name();
			uPtr->setManager(this);
			//LOG_("Created " << name << " (" << uPtr.get() << ") in Manager (" << this << "): "); AllocationMetrics::printMemoryUsage();
			_systems.emplace(id, std::move(uPtr));
			//LOG_("Emplaced " << name << " in Manager systems: "); AllocationMetrics::printMemoryUsage();
		} else {
			LOG("System with hash code (" << id << ") already exists in Manager (" << this << ")");
		}
	}
	template <typename ...Ts> void swallow(Ts&&... args) {} // wrapper allows calling methods on template pack with void return

	struct SystemFactory {
		template <typename ...Ts> static void call(Manager& manager, Ts&&... args) {
			manager.swallow((manager.createSystem(args), 0)...);
		}
	};
	struct EntityFactory {
		template <typename ...Ts> static Entity* call(Manager& manager, Ts&&... args) {
			Entity* entity = manager.createEntity();
			manager.swallow((entity->addComponent(args), 0)...);
			manager.refreshSystems(entity);
			return entity;
		}
	};

	bool init();
	void updateSystems();
	void refreshSystems(Entity* entity);
	void refreshSystems();
	void refresh();

	Entity* getEntity(EntityID entityID);

	Entity* createTree(float x, float y);
	Entity* createBox(float x, float y);
	Entity* createGhost(float x, float y, float lifetime = 7.0f);
private:
	using SystemID = unsigned int;
	std::map<EntityID, std::unique_ptr<Entity>> _entities;
	std::map<SystemID, std::unique_ptr<BaseSystem>> _systems;
private:
	Entity* createEntity();
	void destroyEntity(EntityID entityID);
};