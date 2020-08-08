#pragma once

#include "BaseSystem.h"

#include "../Entity.h"
#include "../Components.h"
#include "../../Utilities.h"

class Manager;

// TODO: Move getComponents function to Entity handle

template <class... Components>
class System : public BaseSystem {
public:
	System() : manager(nullptr) { // manager set by setManager method to avoid having to call modified defualt constructor with manager parameter from each system
		signature.insert(signature.end(), { typeid(Components).hash_code()... });
	}
	virtual void setManager(Manager* newManager) override final {
		manager = newManager;
	}
	// events
	virtual void onEntityChanged(EntityID id) override final {
		bool hasEntity = entities.count(id) == 1 ? true : false;
		assert(entities.count(id) <= 1 && "Cannot have two entities with matching IDs");
		if (hasMatchingSignature(id)) {
			if (!hasEntity) { // entity didn't exist in system -> add to system
				onEntityCreated(id);
			}
		} else {
			if (hasEntity) { // entity had signature component removed -> remove from system
				onEntityDestroyed(id);
			}
		}
	}
	virtual void onEntityCreated(EntityID id) override final {
		entities.emplace(id);
	}
	virtual void onEntityDestroyed(EntityID id) override final {
		entities.erase(id);
	}
protected:
	Manager* manager;
	EntitySet entities;
	// Returns references to system signature components
	std::tuple<Components&...> getComponents(EntityID id) {
		return manager->getComponentReferences<Components...>(id);
	}
	// Returns pointers to requested components
	template <typename ...Cs>
	std::tuple<Cs*...> getComponents(EntityID id) {
		return manager->getComponents<Cs...>(id);
	}
private:
	Signature signature;
	bool hasMatchingSignature(EntityID id) {
		for (const auto& cId : signature) {
			if (!manager->hasComponent(id, cId)) {
				// if entity signatures do not contain a system signature, they do not match
				return false;
			}
		}
		return true;
	}
};

