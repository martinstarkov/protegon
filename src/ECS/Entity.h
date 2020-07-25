#pragma once

#include "Manager.h"

class Entity {
public:
	Entity();
	Entity(Manager* manager); // for serialization
	Entity(EntityID id, Manager* manager);
	Entity(const Entity&) = default;
	template <typename C>
	void addComponent(C&& component) {
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
	C* getComponent() const {
		assert(_manager);
		return _manager->getComponent<C>(_id);
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
	bool hasComponent(ComponentID cId);
	void destroy();
	EntityID getID();
	const EntityID getID() const;
	void setID(EntityID id);
	Manager* getManager();
private:
	Manager* _manager;
	EntityID _id;
};

namespace Serializer {
	void serialize(nlohmann::json& j, const Entity& o);
	void deserialize(const nlohmann::json& j, Entity& o);
}