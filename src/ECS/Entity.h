#pragma once
#include "Types.h"
//#include "Components/BaseComponent.h"
#include <algorithm>
#include <memory>

class Manager;
class BaseComponent;

class Entity {
public:
	Entity(EntityID id, Manager* manager) : _manager(manager), _id(id) {}

	//Entity(const Entity&) = delete;
	//~Entity() = default;

	void destroy() { _alive = false; }
	bool isAlive() { return _alive; }

	void refreshManager();

	template <typename TComponent, typename... TArgs> void addComponent(TArgs&&... mArgs) {
		if (_components.find(typeid(TComponent).hash_code()) == _components.end()) {
			std::unique_ptr<TComponent> component = std::make_unique<TComponent>(std::forward<TArgs>(mArgs)...);
			component->setEntityID(_id);
			_signature.emplace_back(component->getComponentID());
			_components.emplace(component->getComponentID(), std::move(component));
			refreshManager();
		} else { // TODO: Possibly multiple components of same type in the future

		}
	}

	const EntityID getID() const { return _id; }

	const Signature getSignature() const { return _signature; }

	template <typename TComponent> TComponent* getComponent() {
		auto it = _components.find(typeid(TComponent).hash_code());
		if (it != _components.end()) {
			return static_cast<TComponent*>(it->second.get());
		}
		return nullptr;
	}
private:
	Manager* _manager;
	EntityID _id;
	using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;
	ComponentMap _components;
	Signature _signature;
	bool _alive = true;
};
