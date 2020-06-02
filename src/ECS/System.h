#pragma once
#include "BaseSystem.h"
#include "Entity.h"
#include "Components.h"
#include <vector>
#include <tuple>
#include <cstddef>
#include <map>
#include <iostream>

template <class... Components>
class System : public BaseSystem {
protected:
	using ComponentTuple = std::tuple<std::add_pointer_t<Components>...>;
	std::vector<ComponentTuple> _components;
public:
	System(Manager* manager) : BaseSystem(manager) {}
	virtual void onEntityCreated(Entity* entity) override final;
	virtual void onEntityDestroyed(EntityID entityID) override final;
private:
	using EntityIDToIndexMap = std::map<EntityID, std::size_t>;
	EntityIDToIndexMap _entityIDToIndexMap;
private:
	template <std::size_t INDEX, class ComponentType, class... ComponentArgs> 
	bool processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill);
	template <std::size_t INDEX> 
	bool processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill);
};

template<class... Components>
void System<Components...>::onEntityCreated(Entity* entity) {
	ComponentTuple componentTuple;
	std::size_t matchingComponents = 0;
	for (auto& componentPair : entity->getComponents()) {
		if (processEntityComponent<0, Components...>(componentPair.first, componentPair.second, componentTuple)) {
			++matchingComponents;
			if (matchingComponents == sizeof...(Components)) {
				_components.emplace_back(std::move(componentTuple));
				_entityIDToIndexMap.emplace(entity->getID(), _components.size() - 1);
				std::cout << "Emplacing " << entity->getID() << std::endl; // figure out why emplacing happens twice sometimes
				break;
			}
		}
	}
}

template<class ...Components>
inline void System<Components...>::onEntityDestroyed(EntityID entityID) {
	const auto it = _entityIDToIndexMap.find(entityID);
	if (it != _entityIDToIndexMap.end()) {
		std::cout << "1" << std::endl;
		_components[it->second] = std::move(_components.back());
		std::cout << "2" << std::endl;
		_components.pop_back();
		std::cout << "3" << std::endl;
		Component* movedComponent = std::get<0>(_components[it->second]); // ghost3 doesn't get through this step // debug assert failure
		std::cout << "4" << std::endl;
		auto movedTupleIt = _entityIDToIndexMap.find(movedComponent->getEntityID());
		std::cout << "5" << std::endl;
		if (movedTupleIt != _entityIDToIndexMap.end()) {
			std::cout << "Moving" << std::endl;
			movedTupleIt->second = it->second;
		}
	}
}

template<class... Components>
template<std::size_t INDEX, class ComponentType, class ...ComponentArgs>
bool System<Components...>::processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill) {
	if (ComponentType::ID == componentID) {
		std::get<INDEX>(tupleToFill) = static_cast<ComponentType*>(component);
		return true;
	} else {
		return processEntityComponent<INDEX + 1, ComponentArgs...>(componentID, component, tupleToFill);
	}
}

template<class... Components>
template<std::size_t INDEX>
bool System<Components...>::processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill) {
	return false;
}
