#pragma once

#include <algorithm>
#include <memory>

#include "Types.h"
#include "../Vec2D.h"
#include "Components/Component.h"
#include "Components.h"

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
public:
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
		if (_components.find(component.getComponentID()) == _components.end()) {
			std::unique_ptr<TComponent> uPtr = std::make_unique<TComponent>(std::move(component));
			LOG("Added " << name << "(" << sizeof(TComponent) + sizeof(uPtr->getComponentID()) << ") to Entity[" << _id << "]");
			_signature.emplace_back(uPtr->getComponentID());
			_components.emplace(uPtr->getComponentID(), std::move(uPtr));
		} else { // Currently just overrides the component
			// TODO: Possibly multiple components of same type in the future
			std::unique_ptr<TComponent> uPtr = std::make_unique<TComponent>(std::move(component));
			LOG("Replaced " << name << "(" << sizeof(TComponent) + sizeof(uPtr->getComponentID()) << ") in Entity[" << _id << "]");
			_components[uPtr->getComponentID()] = std::move(uPtr);
		}
	}
	template <typename TComponent> void removeEntityComponent() {
		ComponentID id = typeid(TComponent).hash_code();
		auto iterator = _components.find(id);
		if (iterator != _components.end()) {
			const char* name = typeid(TComponent).name();
			LOG("Removed " << name << "(" << sizeof(TComponent) + sizeof(id) << ") from Entity[" << _id << "]");
			resetRelatedComponents<TComponent>(id);
			_components.erase(iterator);
			_signature.erase(std::remove(_signature.begin(), _signature.end(), id), _signature.end());
		}
	}
	template <typename TComponent> void resetRelatedComponents(ComponentID id) {
		// // Reset sprite to original animation state when animation component is removed
		//if (id == typeid(AnimationComponent).hash_code()) {
		//	SpriteComponent* sprite = getComponent<SpriteComponent>();
		//	if (sprite) {
		//		AnimationComponent* animation = getComponent<AnimationComponent>();
		//		//sprite->_source.x = sprite->_source.w * (animation->_state % sprite->_sprites);
		//	}
		//}
	}
private:
	using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;
	Manager* _manager;
	EntityID _id;
	ComponentMap _components;
	Signature _signature;
	bool _alive = true;
};
