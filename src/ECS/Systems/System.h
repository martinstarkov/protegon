#pragma once

#include "BaseSystem.h"

class Manager;

template <class... Cs>
class System : public BaseSystem {
public:
	System() : manager(nullptr) { // manager set by setManager method to avoid having to call modified defualt constructor with manager parameter from each system
		signature.insert(signature.end(), { typeid(Cs).hash_code()... });
	}
	virtual void setManager(Manager* newManager) override final {
		manager = newManager;
	}
	virtual void onEntityChanged(EntityID id) override final {
		bool hasEntity = entities.count(id) == 1 ? true : false;
		if (hasMatchingSignature(id)) {
			if (!hasEntity) { // entity didn't exist in system -> add to system
				entities.emplace(id);
			}
		} else {
			if (hasEntity) { // entity had signature component removed -> remove from system
				onEntityDestroyed(id);
			}
		}
	}
	virtual void onEntityDestroyed(EntityID id) override final {
		entities.erase(id);
	}
protected:
	Manager* manager;
	EntitySet entities;
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

