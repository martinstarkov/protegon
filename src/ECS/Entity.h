#pragma once

#include "Manager.h"

class Entity {
public:
	Entity() : _id(UINT_MAX), _manager(nullptr) {}
	Entity(EntityID id, Manager* manager) : _id(id), _manager(manager) {}
	template <typename C>
	void addComponent(C& component) {
		assert(_manager);
		_manager->addComponents(_id, std::forward<C>(component));
	}
	template <typename ...Cs>
	void addComponents(Cs&&... components) {
		assert(_manager);
		_manager->addComponents(_id, std::forward<Cs>(components)...);
	}
	template <typename C>
	void removeComponent() {
		assert(_manager);
		_manager->removeComponents<C>(_id);
	}
	template <typename ...Cs>
	void removeComponents() {
		assert(_manager);
		_manager->removeComponents<Cs...>(_id);
	}
	template <typename C>
	C* getComponent() {
		assert(_manager);
		return _manager->getComponent<C>(_id);
	}
	template <typename C>
	bool hasComponent() {
		assert(_manager);
		return _manager->hasComponent<C>(_id);
	}
	bool hasComponent(ComponentID cId) {
		assert(_manager);
		return _manager->hasComponent(_id, cId);
	}
	void destroy() {
		assert(_manager);
		_manager->destroyEntity(_id);
	}
	EntityID getID() {
		return _id;
	}
private:
	Manager* _manager;
	EntityID _id;
};