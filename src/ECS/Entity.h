#pragma once
#include "Types.h"
#include <map>
#include <iostream>

class Component;

class Entity {
private:
	using ComponentMap = std::map<ComponentID, Component*>;
	EntityID _id;
	ComponentMap _components;
public:
	Entity(EntityID id) : _id(id) {}

	Entity(const Entity&) = delete;
	Entity(Entity&&) = default;
	Entity& operator=(const Entity&) = default; // used to be delete
	Entity& operator=(Entity&&) = default;
	~Entity() = default;

	template <typename TComponent> void addComponent(TComponent* component) {
		if (_components.find(TComponent::ID) == _components.end()) {
			std::cout << "Adding: " << TComponent::ID << std::endl;
			_components.emplace(TComponent::ID, component);
		}
	}

	EntityID getID() const { return _id; }

	const ComponentMap getComponents() const { return _components;  }

	template <typename ComponentType>
	ComponentType* getComponent() const {
		auto it = _components.find(ComponentType::id);
		if (it != _components.end()) {
			return it->second;
		}
		return nullptr;
	}
};

