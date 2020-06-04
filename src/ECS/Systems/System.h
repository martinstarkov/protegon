#pragma once

#include <algorithm>

#include "BaseSystem.h"

#include "../Components.h"
#include "../Entity.h"

static void printSignature(const Signature& s) {
	for (auto& signature : s) {
		std::cout << signature << ",";
	}
	std::cout << std::endl;
}

template <class... Components>
class System : public BaseSystem {
public:
	System() : _manager(nullptr) { // _manager set by setManager method to avoid having to call modified defualt constructor with manager parameter from each system
		_signature.insert(_signature.end(), { typeid(Components).hash_code()... });
	}
	~System() {
		_manager = nullptr;
		_entities.clear();
		_signature.clear();
	}
	virtual void setManager(Manager* manager) override final {
		_manager = manager;
	}
	virtual void onEntityChanged(Entity* entity) override final { // entity had a component added / was created
		bool existingEntity = containsEntity(entity);
		if (signaturesMatch(entity)) {
			if (!existingEntity) { // entity didn't exist in system
				//std::cout << "Adding entity " << entity->getID() << " to system signature: ";
				//printSignature(_signature);
				_entities.emplace_back(entity);
			}
		} else {
			if (existingEntity) { // entity had 'vital' component removed -> remove from system
				//std::cout << "Removing entity " << entity->getID() << " from system signature: ";
				//printSignature(_signature);
				_entities.erase(std::remove(_entities.begin(), _entities.end(), entity), _entities.end());
			}
		}
	}
	virtual void onEntityDestroyed(EntityID entityID) override final {
		Entity* entity = _manager->getEntity(entityID);
		assert(entity != nullptr);
		auto iterator = std::find(_entities.begin(), _entities.end(), entity);
		if (iterator != _entities.end()) {
			_entities.erase(iterator);
		}
	}
	virtual bool containsEntity(Entity* entity) override final {
		if (std::find(_entities.begin(), _entities.end(), entity) != _entities.end()) {
			return true;
		}
		return false;
	}
private:
	bool signaturesMatch(Entity* entity) {
		for (const auto& componentID : _signature) {
			const Signature entitySignature = entity->getSignature();
			if (!std::count(entitySignature.begin(), entitySignature.end(), componentID)) {
				return false; // if entity signatures do not contain a system signature, they do not match
			}
		}
		return true;
	}
protected:
	Manager* _manager;
	std::vector<Entity*> _entities;
	Signature _signature;
};

