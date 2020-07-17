#pragma once

#include <algorithm>
#include <memory>

#include "Types.h"
#include "../Vec2D.h"
#include "Components/Component.h"
#include "Components.h"

// TODO: In addEntityComponent the Component init() method should be called only after all components have been initialized (if more than one component is being created)
// If only one component is being created, init() will be called on that one, if multiple, they will be created first, then init will be called on them all

// TODO: Replace entity with a handle

class Manager;
class BaseComponent;

class Entity {
public:
	Entity(EntityID id, Manager* manager) : _manager(manager), _id(id), _alive(true) {}
	void destroy() { _alive = false; }
	bool isAlive() { return _alive; }
	const EntityID getID() const { return _id; }
	const Signature getSignature() const { return _signature; }
	void refreshManager(); // wrapper so that Manager.h can be included in .cpp
	void listComponents() {
		LOG("Entity " << _id << ": {");
		for (auto& pair : _components) {
			LOG(pair.second->getComponentName());
		}
		LOG("}");
	}
	template <typename ...Ts> void addComponents(Ts&&... args) {
		swallow((addEntityComponent(args), 0)...);
		refreshManager();
	}
	template <typename ...Ts> void removeComponents() {
		swallow((removeEntityComponent<Ts>(), 0)...);
		refreshManager();
	}
	template <typename TComponent> TComponent* getComponent() {
		auto iterator = _components.find(typeid(TComponent).hash_code());
		// Consider adding assertion here
		if (iterator != _components.end()) {
			return static_cast<TComponent*>(iterator->second.get()); // if these raw pointers get me in trouble with system methods calling deleted objects I swear to god...
		}
		return nullptr;
	}
	template <typename Signature> void addSignature() {
		_signature.emplace_back(static_cast<ComponentID>(typeid(Signature).hash_code()));
	}
	template <typename Signature> void removeSignature() {
		ComponentID id = static_cast<ComponentID>(typeid(Signature).hash_code());
		_signature.erase(std::remove(_signature.begin(), _signature.end(), id), _signature.end());
	}
private:
	template <typename TComponent> void addEntityComponent(TComponent& component) { // make sure to call manager.refreshSystems(Entity*) after this function, wherever it is used
		const char* name = typeid(TComponent).name();
		std::unique_ptr<TComponent> uPtr = std::make_unique<TComponent>(std::move(component));
		uPtr->setParentEntity(this);
		uPtr->init();
		if (_components.find(component.getComponentID()) == _components.end()) {
			//LOG_("Added ");
			_signature.emplace_back(uPtr->getComponentID());
			_components.emplace(uPtr->getComponentID(), std::move(uPtr));
		} else { // Currently just overrides the component
			// TODO: Possibly multiple components of same type in the future
			//LOG_("Replaced ");
			_components[uPtr->getComponentID()] = std::move(uPtr);
		}
		//LOG(<< name << "(" << sizeof(TComponent) + sizeof(uPtr->getComponentID()) << ") -> Entity[" << _id << "]");
	}
	template <typename TComponent> void removeEntityComponent() {
		ComponentID id = typeid(TComponent).hash_code();
		auto iterator = _components.find(id);
		if (iterator != _components.end()) {
			const char* name = typeid(TComponent).name();
			//LOG("Removed " << name << "(" << sizeof(TComponent) + sizeof(id) << ") from Entity[" << _id << "]");
			resetRelatedComponents<TComponent>(id);
			_components.erase(iterator);
			_signature.erase(std::remove(_signature.begin(), _signature.end(), id), _signature.end());
		}
	}
	template <typename TComponent> void resetRelatedComponents(ComponentID id) {
		// Reset sprite to original animation state when animation component is removed
		/*
		if (id == typeid(AnimationComponent).hash_code()) {
			SpriteComponent* sprite = getComponent<SpriteComponent>();
			if (sprite) {
				AnimationComponent* animation = getComponent<AnimationComponent>();
				//sprite->_source.x = sprite->_source.w * (animation->_state % sprite->_sprites);
			}
		}
		*/
	}
private:
	Manager* _manager;
	EntityID _id;
	ComponentMap _components;
	Signature _signature;
	bool _alive;
};
