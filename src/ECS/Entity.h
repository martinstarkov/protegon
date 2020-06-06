#pragma once

#include <algorithm>
#include <memory>

#include "Types.h"
#include "../Vec2D.h"
#include "Components/TransformComponent.h"
#include "Components/Component.h"

class Manager;
class BaseComponent;

class Entity {
public:
	Entity(EntityID id, Manager* manager) : _manager(manager), _id(id) {}

	void destroy() { _alive = false; }
	bool isAlive() { return _alive; }
	const EntityID getID() const { return _id; }
	const Signature getSignature() const { return _signature; }

	void refreshManager(); // wrapper so that Manager.h can be included in .cpp

	template <typename TComponent> void addComponent(TComponent& component) { // make sure to call manager.refreshSystems(Entity*) after this function, wherever it is used
		if (_components.find(component.getComponentID()) == _components.end()) {
			std::unique_ptr<TComponent> uPtr = std::make_unique<TComponent>(std::move(component));
			const char* name = typeid(TComponent).name();
			LOG_("(" << sizeof(TComponent) << ") Created " << name << ": "); AllocationMetrics::printMemoryUsage();
			_signature.emplace_back(uPtr->getComponentID());
			LOG_("(" << sizeof(uPtr->getComponentID()) << ") Emplaced " << name << " into entity signatures: "); AllocationMetrics::printMemoryUsage();
			_components.emplace(uPtr->getComponentID(), std::move(uPtr));
			LOG_("Emplaced " << name << " into entity components: "); AllocationMetrics::printMemoryUsage();
		} else { // TODO: Possibly multiple components of same type in the future

		}
	}
	template <typename TComponent> TComponent* getComponent() {
		auto iterator = _components.find(typeid(TComponent).hash_code());
		if (iterator != _components.end()) {
			return static_cast<TComponent*>(iterator->second.get());
		}
		return nullptr;
	}

	// TODO add "add" function to entity, with component factory which allows adding many components at once to an already created entity

private:
	Manager* _manager;
	EntityID _id;
	using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;
	ComponentMap _components;
	Signature _signature;
	bool _alive = true;
};
