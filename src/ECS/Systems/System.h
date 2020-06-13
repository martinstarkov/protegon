#pragma once

#include <algorithm>

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
	System() : _manager(nullptr) { // _manager set by setManager method to avoid having to call modified defualt constructor with manager parameter from each system
		_signature.insert(_signature.end(), { typeid(Components).hash_code()... });
	}
	virtual void setManager(Manager* manager) override final {
		_manager = manager;
	}
	virtual Entity& getEntity(EntityID entityID) override final {
		return _manager->getEntity(entityID);
	}
	virtual void onEntityChanged(EntityID entityID) override final {
		bool existingEntity = containsEntity(entityID);
		if (signaturesMatch(entityID)) {
			if (!existingEntity) { // entity didn't exist in system
				//LOG_("Adding entity " << entity->getID() << " to system signature: ");
				//printSignature(_signature);
				_entities.emplace_back(entityID);
			}
		} else {
			if (existingEntity) { // entity had 'vital' component removed -> remove from system
				//LOG_("Removing entity " << entity->getID() << " from system signature: ");
				//printSignature(_signature);
				_entities.erase(std::remove(_entities.begin(), _entities.end(), entityID), _entities.end());
			}
		}
	}
	virtual void onEntityDestroyed(EntityID entityID) override final {
		_entities.erase(std::remove(_entities.begin(), _entities.end(), entityID), _entities.end());
	}
protected:
	Manager* _manager;
	std::vector<EntityID> _entities;
	Signature _signature;
private:
	bool containsEntity(EntityID entityID) {
		if (std::find(_entities.begin(), _entities.end(), entityID) != _entities.end()) {
			return true;
		}
		return false;
	}
	bool signaturesMatch(EntityID entityID) {
		for (const auto& componentID : _signature) {
			const Signature& entitySignature = _manager->getEntity(entityID).getSignature();
			if (!std::count(entitySignature.begin(), entitySignature.end(), componentID)) {
				return false; // if entity signatures do not contain a system signature, they do not match
			}
		}
		return true;
	}
};

