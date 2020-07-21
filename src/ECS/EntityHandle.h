#pragma once

#include "Manager.h"

class EntityHandle {
public:
	EntityHandle() : id(UINT_MAX), manager(nullptr) {}
	EntityHandle(EntityID id, Manager* manager) : id(id), manager(manager) {}
	template <typename C>
	void addComponent(C& component) {
		assert(manager);
		manager->addComponents(id, std::forward<C>(component));
	}
	template <typename ...Cs>
	void addComponents(Cs&&... components) {
		assert(manager);
		manager->addComponents(id, std::forward<Cs>(components)...);
	}
	template <typename C>
	void removeComponent() {
		assert(manager);
		manager->removeComponents<C>(id);
	}
	template <typename ...Cs>
	void removeComponents() {
		assert(manager);
		manager->removeComponents<Cs...>(id);
	}
	template <typename C>
	C* getComponent() {
		assert(manager);
		return manager->getComponent<C>(id);
	}
	template <typename C>
	bool hasComponent() {
		assert(manager);
		return manager->hasComponent<C>(id);
	}
	bool hasComponent(ComponentID cId) {
		assert(manager);
		return manager->hasComponent(id, cId);
	}
	void destroy() {
		assert(manager);
		manager->destroyEntity(id);
	}
private:
	Manager* manager;
	EntityID id;
};