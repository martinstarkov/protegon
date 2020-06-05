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

	void refreshManager();

	template <typename TComponent, typename... TArgs> void addComponent(TArgs&&... mArgs) {
		if (_components.find(typeid(TComponent).hash_code()) == _components.end()) {
			std::unique_ptr<TComponent> component = std::make_unique<TComponent>(std::forward<TArgs>(mArgs)...);
			LOG_("(" << sizeof(TransformComponent) << ") Component unique ptr created: "); AllocationMetrics::printMemoryUsage();
			_signature.emplace_back(component->getComponentID());
			LOG_("(" << sizeof(component->getComponentID()) << ") _signature emplaced with ID: "); AllocationMetrics::printMemoryUsage();
			_components.emplace(component->getComponentID(), std::move(component));
			LOG_("_components emplaced with ptr: "); AllocationMetrics::printMemoryUsage();
			refreshManager();
		} else { // TODO: Possibly multiple components of same type in the future

		}
	}

	const EntityID getID() const { return _id; }

	const Signature getSignature() const { return _signature; }

	template <typename TComponent> TComponent* getComponent() {
		auto iterator = _components.find(typeid(TComponent).hash_code());
		if (iterator != _components.end()) {
			return static_cast<TComponent*>(iterator->second.get());
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
