#pragma once

#include <map>
#include <vector>
#include <memory>

#include "Types.h"
#include "../Vec2D.h"

#include "Systems/BaseSystem.h"
#include "Components/BaseComponent.h"

// TODO: Big overhaul of the system and entity factories
// Consider storing components in manager under EntityIDs as opposed to in Entity object
// This will eliminate the need for entity pointers, change all parent relationships in components and states to pass a Manager reference and an EntityID instead of an entity pointer)

struct Entity {
	ComponentMap components;
	bool alive = true;
};

class Manager {
public:
	Manager() = default;
	~Manager() = default;
	Manager(const Manager&) = delete;
	Manager(Manager&&) = delete;

	void init();
	void update();
	void render();

	void entityChanged(EntityID id);
	void entityDestroyed(EntityID id);
	void refresh();
	void refreshDeleted();

	EntityID createEntity();
	void destroyEntity(EntityID entityID);
	bool hasEntity(EntityID entityID);

	EntityID createTree(Vec2D position);
	EntityID createBox(Vec2D position);
	EntityID createPlayer(Vec2D position);

	template <typename ...Cs>
	void addComponents(EntityID id, Cs&&... components) {
		auto it = _entities.find(id);
		if (it != _entities.end()) {
			Util::swallow((addComponent(id, components), 0)...);
			Signature added;
			added.insert(added.end(), { typeid(Cs).hash_code()... });
			for (const ComponentID& cId : added) {
				auto cIt = it->second->components.find(cId);
				assert(cIt != it->second->components.end() && "Cannot call init() on components which were unsuccesfully added to entity");
				cIt->second->init();
			}
			entityChanged(id);
		}
	}
	template <typename ...Cs>
	void removeComponents(EntityID id) {
		auto it = _entities.find(id);
		if (it != _entities.end()) {
			Util::swallow((removeComponent<Cs>(id), 0)...);
			entityChanged(id);
		}
	}
	template <typename C>
	C* getComponent(EntityID id) {
		auto it = _entities.find(id);
		if (it != _entities.end()) {
			ComponentID cId = static_cast<ComponentID>(typeid(C).hash_code());
			ComponentMap& components = it->second->components;
			auto cIt = components.find(cId);
			if (cIt != components.end()) {
				return static_cast<C*>(cIt->second.get());
			}
		}
		return nullptr;
	}
	template <typename C>
	bool hasComponent(EntityID id) {
		return getComponent<C>(id) != nullptr;
	}
	bool hasComponent(EntityID id, ComponentID cId);
	template <typename ...Ss>
	void createSystems(Ss&&... systems) {
		Util::swallow((createSystem(systems), 0)...);
	}
	template <typename S>
	S* getSystem() {
		SystemID sId = static_cast<SystemID>(typeid(S).hash_code());
		auto it = _systems.find(sId);
		if (it != _systems.end()) {
			return static_cast<S*>(it->second.get());
		}
		return nullptr;
	}
private:
	template <typename C>
	void addComponent(EntityID id, C& component) {
		auto it = _entities.find(id);
		if (it != _entities.end()) {
			ComponentID cId = static_cast<ComponentID>(typeid(C).hash_code());
			std::unique_ptr<C> uPtr = std::make_unique<C>(std::move(component));
			uPtr->setHandle(id, this);
			ComponentMap& components = it->second->components;
			if (components.find(cId) == components.end()) { // Add new component
				components.emplace(cId, std::move(uPtr));
				LOG_("Added");
			} else { // Replace old component
				// TODO: Possibly in the future include support for multiple components of the same type
				components[cId] = std::move(uPtr);
				LOG_("Replaced");
			}
		}
		LOG(" " << typeid(C).name() << " (" << sizeof(C) << ") -> Entity [" << id << "]");
	}
	template <typename C>
	void removeComponent(EntityID id) {
		auto it = _entities.find(id);
		if (it != _entities.end()) {
			ComponentID cId = static_cast<ComponentID>(typeid(C).hash_code());
			ComponentMap& components = it->second->components;
			auto cIt = components.find(cId);
			if (cIt != components.end()) {
				components.erase(cIt);
				LOG("Removed " << typeid(C).name() << " (" << sizeof(C) << ") from Entity [" << id << "]");
			}
		}
	}
	template <typename S>
	void createSystem(S& system) {
		SystemID sId = static_cast<SystemID>(typeid(S).hash_code());
		assert(_systems.find(sId) == _systems.end());
		std::unique_ptr<S> uPtr = std::make_unique<S>(std::move(system));
		uPtr->setManager(this);
		_systems.emplace(sId, std::move(uPtr));
	}
	std::map<EntityID, std::unique_ptr<Entity>> _entities;
	std::map<SystemID, std::unique_ptr<BaseSystem>> _systems;
};