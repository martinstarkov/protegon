#pragma once

#include <map>
#include <vector>
#include <memory>

#include "Types.h"
#include "../Vec2D.h"

#include "Entity.h"
#include "Systems/BaseSystem.h"
#include "Systems.h"
#include "Components.h"

class Manager {
public:
	Manager() = default;
	~Manager() = default;
	Manager(const Manager&) = delete;
	Manager(Manager&&) = delete;
	bool init();
	void updateSystems();
	void refreshSystems(const EntityID entity);
	void refreshSystems();
	void refresh();
	Entity& getEntity(EntityID entityID);
	EntityID createTree(double x, double y);
	EntityID createBox(double x, double y);
	EntityID createGhost(double x, double y, double lifetime = 7.0);

	struct SystemFactory {
		template <typename ...Ts> static void call(Manager& manager, Ts&&... args) {
			swallow((manager.createSystem(args), 0)...);
		}
	};
	struct EntityFactory {
		template <typename ...Ts> static EntityID call(Manager& manager, Ts&&... args) {
			Entity& entity = manager.createEntity();
			entity.addComponents(std::forward<Ts>(args)...);
			return entity.getID();
		}
	};

	template<class TFunctor, typename... Ts> auto create(Ts&&... args) {
		return TFunctor::call(*this, std::forward<Ts>(args)...);
	}
	template <typename TSystem> TSystem* getSystem() {
		auto iterator = _systems.find(static_cast<SystemID>(typeid(TSystem).hash_code()));
		assert(iterator != _systems.end() && "Attempting to get non-existent system");
		return static_cast<TSystem*>(iterator->second.get());
	}
	template <typename TSystem> void createSystem(TSystem& system) {
		SystemID id = static_cast<SystemID>(typeid(TSystem).hash_code());
		assert(_systems.find(id) == _systems.end());
		std::unique_ptr<TSystem> uPtr = std::make_unique<TSystem>(std::move(system));
		const char* name = typeid(TSystem).name();
		uPtr->setManager(this);
		_systems.emplace(id, std::move(uPtr));
	}
private:
	Entity& createEntity();
	void destroyEntity(EntityID entityID);
	std::map<EntityID, std::unique_ptr<Entity>> _entities;
	std::map<SystemID, std::unique_ptr<BaseSystem>> _systems;
};