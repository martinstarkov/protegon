#pragma once

#include <vector>
#include <tuple>
#include <cstddef>
#include <map>
#include <iostream>
#include <utility>

#include "./ECS/Entity.h"
#include "./ECS/Systems/BaseSystem.h"
#include "./ECS/Components.h"

template <class... Components>
class System : public BaseSystem {
protected:
	using ComponentTuple = std::tuple<std::add_pointer_t<Components>...>;
	Entities _entities;
public:
	virtual void onEntityCreated(Entity* entity) override final;
	virtual void onEntityDestroyed(EntityID entityID) override final;
private:
	template <std::size_t INDEX, class ComponentType, class... ComponentArgs> 
	bool processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill);
	template <std::size_t INDEX> 
	bool processEntityComponent(ComponentID componentID, Component* component, ComponentTuple& tupleToFill);
};

//template<size_t I = 0, typename... Tp>
//void print(std::tuple<Tp...>& t) {
//	std::cout << typeid(*std::get<I>(t)).name() << " ";
//	if constexpr (I + 1 != sizeof...(Tp))
//		print<I + 1>(t);
//}

template<class... Components>
void System<Components...>::onEntityCreated(Entity* entity) {
	ComponentTuple componentTuple;
	std::size_t matchingComponents = 0;
	for (auto& componentPair : entity->getComponents()) {
		if (processEntityComponent<0, Components...>(componentPair.first, componentPair.second, componentTuple)) {
			++matchingComponents;
			if (matchingComponents == sizeof...(Components)) {
				_entities.emplace(entity->getID(), entity);
				break;
			}
		}
	}
}

template<class ...Components>
inline void System<Components...>::onEntityDestroyed(EntityID entityID) {
	auto it = _entities.find(entityID);
	if (it != _entities.end()) {
		_entities.erase(it);
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
