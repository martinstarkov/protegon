#pragma once

#include <algorithm>
#include <typeinfo>

#include "BaseSystem.h"

#include "../Components.h"

static void printSignature(const Signature& s) {
	for (auto& signature : s) {
		LOG_(signature << ",");
	}
	LOG("");
}

template <class... Components>
class System : public BaseSystem {
public:
	System() : manager(nullptr) { // manager set by setManager method to avoid having to call modified defualt constructor with manager parameter from each system
		signature.insert(signature.end(), { typeid(Components).hash_code()... });
	}
	virtual void setManager(Manager* newManager) override final {
		manager = newManager;
	}
	virtual Entity& getEntity(EntityID entityID) override final {
		return manager->getEntity(entityID);
	}
	virtual void onEntityChanged(EntityID entityID) override final {
		bool existingEntity = containsEntity(entityID);
		if (signaturesMatch(entityID)) {
			if (!existingEntity) { // entity didn't exist in system
				//LOG_("Adding entity " << entity->getID() << " to system signature: ");
				//printSignature(signature);
				entities.emplace(entityID);
			}
		} else {
			if (existingEntity) { // entity had 'vital' component removed -> remove from system
				//LOG_("Removing entity " << entity->getID() << " from system signature: ");
				//printSignature(signature);
				onEntityDestroyed(entityID);
			}
		}
	}
	virtual void onEntityDestroyed(EntityID entityID) override final {
		entities.erase(entityID);
	}
protected:
	Manager* manager;
	std::set<EntityID> entities;
	Signature signature;
private:
	bool containsEntity(EntityID entityID) {
		std::size_t count = entities.count(entityID);
		assert((count == 0 || count == 1) && "_entities cannot contain more than one unique entityID");
		return count == 1;
	}
	bool signaturesMatch(EntityID entityID) {
		for (const auto& componentID : signature) {
			const Signature& entitySignature = manager->getEntity(entityID).getSignature();
			if (!std::count(entitySignature.begin(), entitySignature.end(), componentID)) {
				return false; // if entity signatures do not contain a system signature, they do not match
			}
		}
		return true;
	}
};

